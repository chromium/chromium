// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_features.h"

namespace chrome_pdf {
namespace features {

BASE_FEATURE(kAccessiblePDFForm,
             "AccessiblePDFForm",
             base::FEATURE_DISABLED_BY_DEFAULT);

// "Incremental loading" refers to loading the PDF as it arrives.
// TODO(crbug.com/1064175): Remove this once incremental loading is fixed.
BASE_FEATURE(kPdfIncrementalLoading,
             "PdfIncrementalLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// "Partial loading" refers to loading only specific parts of the PDF.
// TODO(crbug.com/1064175): Remove this once partial loading is fixed.
BASE_FEATURE(kPdfPartialLoading,
             "PdfPartialLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPdfUseSkiaRenderer,
             "PdfUseSkiaRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature has no effect if Chrome is built with no XFA support.
BASE_FEATURE(kPdfXfaSupport,
             "PdfXfaSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace chrome_pdf
