// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

#include "build/chromeos_buildflags.h"

namespace chrome_pdf {
namespace features {

const base::Feature kAccessiblePDFForm = {"AccessiblePDFForm",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPdfHonorJsContentSettings = {
    "PdfHonorJsContentSettings", base::FEATURE_ENABLED_BY_DEFAULT};

// "Incremental loading" refers to loading the PDF as it arrives.
// TODO(crbug.com/1064175): Remove this once incremental loading is fixed.
const base::Feature kPdfIncrementalLoading = {
    "PdfIncrementalLoading", base::FEATURE_DISABLED_BY_DEFAULT};

// "Partial loading" refers to loading only specific parts of the PDF.
// TODO(crbug.com/1064175): Remove this once partial loading is fixed.
const base::Feature kPdfPartialLoading = {"PdfPartialLoading",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPDFViewerUpdate = {"PDFViewerUpdate",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPdfViewerPresentationMode = {
    "PdfViewerPresentationMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSaveEditedPDFForm = {"SaveEditedPDFForm",
#if BUILDFLAG(IS_ASH)
                                          base::FEATURE_ENABLED_BY_DEFAULT
#else
                                          base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ASH)
};

const base::Feature kTabAcrossPDFAnnotations = {
    "TabAcrossPDFAnnotations", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace chrome_pdf
