// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_unsupported_features.h"

#include "base/check.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "third_party/pdfium/public/fpdf_ext.h"

namespace chrome_pdf {

namespace {

// Only check `g_engine_for_unsupported` if `g_engine_available` is true.
bool g_engine_available = true;
PDFiumEngine* g_engine_for_unsupported = nullptr;

void Unsupported_Handler(UNSUPPORT_INFO*, int type) {
  if (!g_engine_for_unsupported) {
    DCHECK(!g_engine_available);
    return;
  }

  std::string feature;
  switch (type) {
    case FPDF_UNSP_DOC_XFAFORM:
      feature = "XFA";
      break;
    case FPDF_UNSP_DOC_PORTABLECOLLECTION:
      feature = "Portfolios_Packages";
      break;
    case FPDF_UNSP_DOC_ATTACHMENT:
    case FPDF_UNSP_ANNOT_ATTACHMENT:
      feature = "Attachment";
      break;
    case FPDF_UNSP_DOC_SECURITY:
      feature = "Rights_Management";
      break;
    case FPDF_UNSP_DOC_SHAREDREVIEW:
      feature = "Shared_Review";
      break;
    case FPDF_UNSP_DOC_SHAREDFORM_ACROBAT:
    case FPDF_UNSP_DOC_SHAREDFORM_FILESYSTEM:
    case FPDF_UNSP_DOC_SHAREDFORM_EMAIL:
      feature = "Shared_Form";
      break;
    case FPDF_UNSP_ANNOT_3DANNOT:
      feature = "3D";
      break;
    case FPDF_UNSP_ANNOT_MOVIE:
      feature = "Movie";
      break;
    case FPDF_UNSP_ANNOT_SOUND:
      feature = "Sound";
      break;
    case FPDF_UNSP_ANNOT_SCREEN_MEDIA:
    case FPDF_UNSP_ANNOT_SCREEN_RICHMEDIA:
      feature = "Screen";
      break;
    case FPDF_UNSP_ANNOT_SIG:
      feature = "Digital_Signature";
      break;
  }

  g_engine_for_unsupported->UnsupportedFeature(feature);
}

UNSUPPORT_INFO g_unsupported_info = {1, Unsupported_Handler};

}  // namespace

void InitializeUnsupportedFeaturesHandler() {
  FSDK_SetUnSpObjProcessHandler(&g_unsupported_info);
}

ScopedUnsupportedFeature::ScopedUnsupportedFeature(PDFiumEngine* engine)
    : saved_engine_available_(g_engine_available),
      saved_engine_(g_engine_for_unsupported) {
  DCHECK(engine);
  g_engine_available = true;
  g_engine_for_unsupported = engine;
}

ScopedUnsupportedFeature::ScopedUnsupportedFeature(NoEngine no_engine)
    : saved_engine_available_(g_engine_available),
      saved_engine_(g_engine_for_unsupported) {
  g_engine_available = false;
  g_engine_for_unsupported = nullptr;
}

ScopedUnsupportedFeature::~ScopedUnsupportedFeature() {
  g_engine_for_unsupported = saved_engine_;
  g_engine_available = saved_engine_available_;
}

}  // namespace chrome_pdf
