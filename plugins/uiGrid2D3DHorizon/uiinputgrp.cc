#include "uiinputgrp.h"

#include "uibutton.h"
#include "uigeninput.h"
#include "uicombobox.h"
#include "uiioobjsel.h"
#include "uimsg.h"
#include "uipossubsel.h"
#include "uiioobjmanip.h"
#include "uiseis2dlineselgrp.h"
#include "bufstringset.h"
#include "emsurfacetr.h"
#include "emioobjinfo.h"
#include "emmanager.h"
#include "emhorizon3d.h"
#include "emhorizon2d.h"
#include "emsurfacegeometry.h"
#include "typeset.h"
#include "ranges.h"
#include "survinfo.h"
#include "uipolygonparsel.h"

#include "survgeom2d.h"
#include "trckeyzsampling.h"
#include "wmgridder2d.h"

uiInputGrp::uiInputGrp( uiParent* p, bool has2Dhorizon, bool has3Dhorizon )
: uiDlgGroup(p, tr("Input Data")), hor2Dfld_(nullptr), lines2Dfld_(nullptr),
  hor3Dfld_(nullptr), subsel3Dfld_(nullptr)
{
    uiObject* lastfld = nullptr;

    if (has2Dhorizon) {
	hor2Dfld_ = new uiIOObjSel(this, EMHorizon2DTranslatorGroup::ioContext(), uiStrings::s2DHorizon());
	mAttachCB(hor2Dfld_->selectionDone, uiInputGrp::hor2DselCB);

        lines2Dfld_ = new WMLib::uiSeis2DLineSelGrp( this, OD::ChooseZeroOrMore );
        lines2Dfld_->attach( alignedBelow, hor2Dfld_ );

        lastfld = (uiObject*) lines2Dfld_;
    }
    if (has3Dhorizon) {
        exp3D_ = new uiCheckBox(this, tr("Include 3D horizon"));
        if (lastfld!=nullptr)
            exp3D_->attach(alignedBelow, lastfld);
        exp3D_->setChecked(true);
	mAttachCB(exp3D_->activated, uiInputGrp::exp3DselCB);

        hor3Dfld_ = new uiIOObjSel(this, EMHorizon3DTranslatorGroup::ioContext(), uiStrings::s3DHorizon());
        hor3Dfld_->attach(alignedBelow, exp3D_);
	mAttachCB(hor3Dfld_->selectionDone, uiInputGrp::hor3DselCB);

        subsel3Dfld_ = new uiPosSubSel( this, uiPosSubSel::Setup(false,false) );
        subsel3Dfld_->attach( alignedBelow, hor3Dfld_ );

	lastfld = (uiObject*) subsel3Dfld_;
    }

    contpolyfld_ = new WMLib::uiPolygonParSel(this, tr("Contour Polygons/Polylines"), true);
    contpolyfld_->attach( alignedBelow, lastfld );

    update();
}

uiInputGrp::~uiInputGrp()
{
    detachAllNotifiers();
}

void uiInputGrp::exp3DselCB(CallBacker*)
{
    if (exp3D_ && hor3Dfld_ && subsel3Dfld_) {
	hor3Dfld_->setChildrenSensitive(exp3D_->isChecked());
	subsel3Dfld_->setChildrenSensitive(exp3D_->isChecked());
    }
}

bool uiInputGrp::fillPar( IOPar& par ) const
{
    MultiID hor2Did, hor3Did;
    getHorIds(hor2Did, hor3Did);
    if (!hor2Did.isUdf()) {
        TypeSet<Pos::GeomID> mids;
        getGeoMids(mids);
        if (mids.size()>0) {
            par.set( wmGridder2D::sKey2DHorizonID(), hor2Did );
            par.set( wmGridder2D::sKey2DLineIDNr(), mids.size() );
            for (int idx=0; idx<mids.size(); idx++)
                par.set(IOPar::compKey(wmGridder2D::sKey2DLineID(), idx), mids[idx]);
        }
    }
    if (!hor3Did.isUdf() && exp3D_->isChecked()) {
        par.set( wmGridder2D::sKey3DHorizonID(), hor3Did );
        TrcKeyZSampling tkz;
        get3Dsel(tkz);
        tkz.hsamp_.fillPar(par);
    }

    const TypeSet<MultiID>& selpolytids = contpolyfld_->selPolygonIDs();
    par.set( wmGridder2D::sKeyContourPolyNr(), selpolytids.size() );
    for ( int idx=0; idx<selpolytids.size(); idx++ )
	par.set( IOPar::compKey(wmGridder2D::sKeyContourPolyID(),idx), selpolytids[idx] );

    return true;
}

void uiInputGrp::usePar( const IOPar& par )
{
    MultiID hor2Did, hor3Did;

    hor3Did.setUdf();
    if (par.get(wmGridder2D::sKey3DHorizonID(), hor3Did)) {
        if (hor3Dfld_ && subsel3Dfld_ && exp3D_) {
            hor3Dfld_->setInput(hor3Did);
            TrcKeyZSampling tkz;
            tkz.usePar(par);
            subsel3Dfld_->setInput(tkz);
	    exp3D_->setChecked(true);
        }
    } else if (exp3D_)
	exp3D_->setChecked(false);

    exp3DselCB(nullptr);

    hor2Did.setUdf();
    if (par.get(wmGridder2D::sKey2DHorizonID(), hor2Did)) {
        if (hor2Dfld_!=nullptr) {
            int nlines = 0;
            par.get(wmGridder2D::sKey2DLineIDNr(), nlines);
            if (nlines>0) {
                TypeSet<Pos::GeomID> mids;
                for (int idx=0; idx<nlines; idx++) {
                    Pos::GeomID id;
                    if (par.get(IOPar::compKey(wmGridder2D::sKey2DLineID(),idx), id))
                        mids += id;
                }
                lines2Dfld_->setChosen(mids);
            }
        }
    }

    int nrcontpoly = 0;
    contpolyfld_->setEmpty();
    if (par.get(wmGridder2D::sKeyContourPolyNr(), nrcontpoly)) {
	if (nrcontpoly>0) {
	    TypeSet<MultiID> polyIDs;
	    for (int idx=0; idx<nrcontpoly; idx++) {
		MultiID id;
		if (!par.get(IOPar::compKey(wmGridder2D::sKeyContourPolyID(), idx), id))
		    return;
		polyIDs += id;
	    }
	    contpolyfld_->setSelectedPolygons(polyIDs);
	}
    }

}

void uiInputGrp::getHorIds( MultiID& hor2Did, MultiID& hor3Did ) const
{
    hor2Did.setUdf();
    hor3Did.setUdf();
    if (hor2Dfld_!=nullptr) {
        const IOObj* horObj = hor2Dfld_->ioobj(true);
        if (horObj!=nullptr)
            hor2Did = horObj->key();
    }
    if (hor3Dfld_!=nullptr) {
        const IOObj* horObj = hor3Dfld_->ioobj(true);
        if (horObj!=nullptr)
            hor3Did = horObj->key();
    }
}

void uiInputGrp::getInputRange( Interval<int>& inlrg, Interval<int>& crlrg )
{
    MultiID hor2Did, hor3Did;
    getHorIds( hor2Did, hor3Did );
    inlrg.setUdf();
    crlrg.setUdf();

    if (!hor2Did.isUdf() && num2DLinesChosen()>0) {
        EM::IOObjInfo eminfo(hor2Did);
        if (!eminfo.isOK()) {
            ErrMsg("uiInputGrp::getInputRange - reading 2D horizon failed");
            return;
        }
        TypeSet<StepInterval<int>> trcRanges;
        TypeSet<Pos::GeomID> horGeomids;
        if (!eminfo.getTrcRanges(trcRanges) || !eminfo.getGeomIDs(horGeomids)){
            ErrMsg("uiInputGrp::getInputRange - reading 2D horizon trace ranges failed");
            return;
        }
        TypeSet<Pos::GeomID> geomids;
        getGeoMids(geomids);
        for (int idx=0; idx<geomids.size(); idx++) {
            mDynamicCastGet( const Survey::Geometry2D*, geom2d, Survey::GM().getGeometry(geomids[idx]) );
            if ( !geom2d )
                continue;
            int gidx = horGeomids.indexOf(geomids[idx]);
            StepInterval<int> trcrng = trcRanges[gidx];
            Coord pos;
            float spnr;
            geom2d->getPosByTrcNr(trcrng.start, pos, spnr);
            BinID bin = SI().transform(pos);
            if (idx==0) {
                inlrg.start = bin.inl();
                inlrg.stop = bin.inl();
                crlrg.start = bin.crl();
                crlrg.stop = bin.crl();
            } else {
                inlrg.start = bin.inl()<inlrg.start ? bin.inl() : inlrg.start;
                inlrg.stop = bin.inl()>inlrg.stop ? bin.inl() : inlrg.stop;
                crlrg.start = bin.crl()<crlrg.start ? bin.crl() : crlrg.start;
                crlrg.stop = bin.crl()>crlrg.stop ? bin.crl() : crlrg.stop;
            }
            geom2d->getPosByTrcNr(trcrng.stop, pos, spnr);
            bin = SI().transform(pos);
            inlrg.start = bin.inl()<inlrg.start ? bin.inl() : inlrg.start;
            inlrg.stop = bin.inl()>inlrg.stop ? bin.inl() : inlrg.stop;
            crlrg.start = bin.crl()<crlrg.start ? bin.crl() : crlrg.start;
            crlrg.stop = bin.crl()>crlrg.stop ? bin.crl() : crlrg.stop;
        }
    }

    if (!hor3Did.isUdf()) {
        EM::EMObject* obj = EM::EMM().loadIfNotFullyLoaded(hor3Did);
        if (obj==nullptr) {
            ErrMsg("uiInputGrp::getInputRange - loading 3D horizon failed");
            return;
        }
        obj->ref();
        mDynamicCastGet(EM::Horizon3D*,hor,obj);
        if (hor==nullptr) {
            ErrMsg("uiInputGrp::getInputRange - casting 3D horizon failed");
            obj->unRef();
            return;
        }
        mDynamicCastGet( const EM::RowColSurfaceGeometry*, rcGeom, &hor->geometry() );
        StepInterval<int> rR = rcGeom->rowRange();
        StepInterval<int> cR = rcGeom->colRange();
        if (inlrg.isUdf()) {
            inlrg.start = rR.start;
            inlrg.stop = rR.stop;
            crlrg.start = cR.start;
            crlrg.stop = cR.stop;
        } else {
            inlrg.include(rR);
            crlrg.include(cR);
        }
    }
}

int uiInputGrp::num2DLinesChosen()
{
    if (hor2Dfld_!=nullptr && lines2Dfld_!=nullptr)
        return lines2Dfld_->nrChosen();
    else
        return 0;
}

void uiInputGrp::getGeoMids( TypeSet<Pos::GeomID>& geomids ) const
{
    geomids.erase();
    if (lines2Dfld_!=nullptr)
        lines2Dfld_->getChosen(geomids);
}

void uiInputGrp::get3Dsel( TrcKeyZSampling& envelope ) const
{
    envelope.setEmpty();
    if (subsel3Dfld_!=nullptr)
        envelope = subsel3Dfld_->envelope();
}

void uiInputGrp::hor2DselCB(CallBacker* )
{
    const IOObj* horObj = hor2Dfld_->ioobj(true);
    if (!horObj) {
        lines2Dfld_->clear();
    } else {
        EM::IOObjInfo eminfo(horObj->key());
        if (!eminfo.isOK()) {
            BufferString tmp("uiInputGrp::hor2DselCB - cannot read ");
            tmp += horObj->name();
            ErrMsg( tmp );
            return;
        }
        TypeSet<Pos::GeomID> mids;
        getGeoMids(mids);

        BufferStringSet lnms;
        TypeSet<Pos::GeomID> geomids;
        eminfo.getLineNames(lnms);
        eminfo.getGeomIDs(geomids);
        lines2Dfld_->setInput( lnms, geomids );

        lines2Dfld_->setChosen(mids);
    }
}

void uiInputGrp::hor3DselCB(CallBacker*)
{
    const IOObj* horObj = hor3Dfld_->ioobj(true);
    if (horObj) {
        EM::IOObjInfo eminfo(horObj->key());
        if (!eminfo.isOK()) {
            BufferString tmp("uiInputGrp::hor3DselCB - cannot read ");
            tmp += horObj->name();
            ErrMsg( tmp );
            return;
        }
        uiString msg;
        EM::SurfaceIOData emdata;
        if (!eminfo.getSurfaceData( emdata, msg )) {
            BufferString tmp("uiInputGrp::hor3DselCB - cannot read surface data for ");
            tmp += horObj->name();
            tmp += " - ";
            tmp += msg.getOriginalString();
            ErrMsg( tmp );
            return;
        }
        TrcKeyZSampling cs;
        cs.hsamp_ = emdata.rg;
        subsel3Dfld_->setInput( cs );
    }
}

void uiInputGrp::update()
{
    if (hor2Dfld_!=nullptr)
        hor2DselCB(0);
    if (hor3Dfld_!=nullptr) {
        hor3DselCB(0);
        exp3DselCB(0);
    }
}
