// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

#include "base/feature_list.h"
#include "pdf/buildflags.h"

namespace chrome_pdf::features {

namespace {
bool g_is_oopif_pdf_policy_enabled = true;
}  // namespace

BASE_FEATURE(kAccessiblePDFForm,
             "AccessiblePDFForm",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfCr23, "PdfCr23", base::FEATURE_DISABLED_BY_DEFAULT);

// "Incremental loading" refers to loading the PDF as it arrives.
// TODO(crbug.com/40123601): Remove this once incremental loading is fixed.
BASE_FEATURE(kPdfIncrementalLoading,
             "PdfIncrementalLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfOopif, "PdfOopif", base::FEATURE_DISABLED_BY_DEFAULT);

// "Partial loading" refers to loading only specific parts of the PDF.
// TODO(crbug.com/40123601): Remove this once partial loading is fixed.
BASE_FEATURE(kPdfPartialLoading,
             "PdfPartialLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfPortfolio, "PdfPortfolio", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfSearchify, "PdfSearchify", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseSkiaRenderer,
             "PdfUseSkiaRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature has no effect if Chrome is built with no XFA support.
BASE_FEATURE(kPdfXfaSupport,
             "PdfXfaSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_PDF_INK2)
BASE_FEATURE(kPdfInk2, "PdfInk2", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

void SetIsOopifPdfPolicyEnabled(bool is_oopif_pdf_policy_enabled) {
  g_is_oopif_pdf_policy_enabled = is_oopif_pdf_policy_enabled;
}

bool IsOopifPdfEnabled() {
  return g_is_oopif_pdf_policy_enabled &&
         base::FeatureList::IsEnabled(kPdfOopif);
}

}  // namespace chrome_pdf::features
