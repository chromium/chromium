// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "pdf/buildflags.h"

namespace chrome_pdf::features {

namespace {
bool g_is_oopif_pdf_policy_enabled = true;
}  // namespace

BASE_FEATURE(kAccessiblePDFForm,
             "AccessiblePDFForm",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables PDF WebUI save to get PDF content from renderer in blocks.
BASE_FEATURE(kPdfGetSaveDataInBlocks,
             "PdfGetSaveDataInBlocks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Saves original PDFs to disk from the in-memory copy instead of redownloading
// them.
BASE_FEATURE(kPdfSaveOriginalFromMemory,
             "PdfSaveOriginalFromMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfSearchify, "PdfSearchify", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPdfSearchifySave,
             "PdfSearchifySave",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables accessibility tags in PDFs to be parsed and integrated into the
// accessibility tree by Chrome's PDF Viewer. Accessibility tags provide
// structure and semantics to the text found in a PDF, e.g. they could mark a
// specific piece of text as a heading, or a block of text as a paragraph.
BASE_FEATURE(kPdfTags, "PdfTags", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseShowSaveFilePicker,
             "PdfUseShowSaveFilePicker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseSkiaRenderer,
             "PdfUseSkiaRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature has no effect if Chrome is built with no XFA support.
BASE_FEATURE(kPdfXfaSupport,
             "PdfXfaSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_PDF_INK2)
BASE_FEATURE(kPdfInk2, "PdfInk2", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables text annotations.
const base::FeatureParam<bool> kPdfInk2TextAnnotations{
    &kPdfInk2, "text-annotations", false};

// Enables text highlighting with the Ink highlighter brush.
const base::FeatureParam<bool> kPdfInk2TextHighlighting{
    &kPdfInk2, "text-highlighting", false};
#endif

void SetIsOopifPdfPolicyEnabled(bool is_oopif_pdf_policy_enabled) {
  g_is_oopif_pdf_policy_enabled = is_oopif_pdf_policy_enabled;
}

bool IsOopifPdfEnabled() {
  return g_is_oopif_pdf_policy_enabled &&
         base::FeatureList::IsEnabled(kPdfOopif);
}

bool IsPdfSearchifySaveEnabled() {
  return base::FeatureList::IsEnabled(kPdfSearchify) &&
         base::FeatureList::IsEnabled(kPdfSearchifySave);
}

}  // namespace chrome_pdf::features
