// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "pdf/buildflags.h"

namespace chrome_pdf::features {

namespace {
bool g_is_oopif_pdf_policy_enabled = true;
}  // namespace

BASE_FEATURE(kAccessiblePDFForm, base::FEATURE_DISABLED_BY_DEFAULT);

// "Incremental loading" refers to loading the PDF as it arrives.
// TODO(crbug.com/40123601): Remove this once incremental loading is fixed.
BASE_FEATURE(kPdfIncrementalLoading, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfOopif, base::FEATURE_DISABLED_BY_DEFAULT);

// "Partial loading" refers to loading only specific parts of the PDF.
// TODO(crbug.com/40123601): Remove this once partial loading is fixed.
BASE_FEATURE(kPdfPartialLoading, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfPortfolio, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables PDF WebUI save to get PDF content from renderer in blocks.
BASE_FEATURE(kPdfGetSaveDataInBlocks, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfSearchifySave, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables accessibility tags in PDFs to be parsed and integrated into the
// accessibility tree by Chrome's PDF Viewer. Accessibility tags provide
// structure and semantics to the text found in a PDF, e.g. they could mark a
// specific piece of text as a heading, or a block of text as a paragraph.
BASE_FEATURE(kPdfTags, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseShowSaveFilePicker, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseSkiaRenderer, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature has no effect if Chrome is built with no XFA support.
BASE_FEATURE(kPdfXfaSupport, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enables PDFium's version 2 font mapping interface, which uses per-request
// font matching instead of enumerating all fonts upfront. This eliminates the
// need for the ListFamilies() IPC call and improves PDF load performance on
// Linux and ChromeOS. Version 2 makes PDFium call MapFont() directly for each
// font request rather than searching a pre-built font list.
//
// TODO(crbug.com/462403025): Remove this flag and the code that exists only to
// support the version 1 font mapping interface, once this safely rolls out.
BASE_FEATURE(kPdfiumPerRequestFontMatching, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PDF_INK2)
BASE_FEATURE(kPdfInk2, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables text annotations.
const base::FeatureParam<bool> kPdfInk2TextAnnotations{
    &kPdfInk2, "text-annotations", false};

// Enables text highlighting with the Ink highlighter brush.
const base::FeatureParam<bool> kPdfInk2TextHighlighting{
    &kPdfInk2, "text-highlighting", false};
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
// Saves the PDF file to Google Drive.
BASE_FEATURE(kPdfSaveToDrive, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the survey for saving PDF to Google Drive.
BASE_FEATURE(kPdfSaveToDriveSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
// The consumer survey trigger ID.
const base::FeatureParam<std::string> kPdfSaveToDriveSurveyConsumerTriggerId{
    &kPdfSaveToDriveSurvey, "consumer-trigger-id", ""};
// The enterprise survey trigger ID.
const base::FeatureParam<std::string> kPdfSaveToDriveSurveyEnterpriseTriggerId{
    &kPdfSaveToDriveSurvey, "enterprise-trigger-id", ""};
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

void SetIsOopifPdfPolicyEnabled(bool is_oopif_pdf_policy_enabled) {
  g_is_oopif_pdf_policy_enabled = is_oopif_pdf_policy_enabled;
}

bool IsOopifPdfEnabled() {
  return g_is_oopif_pdf_policy_enabled &&
         base::FeatureList::IsEnabled(kPdfOopif);
}

}  // namespace chrome_pdf::features
