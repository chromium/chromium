// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the pdf
// module.

#ifndef PDF_PDF_FEATURES_H_
#define PDF_PDF_FEATURES_H_

#include "base/feature_list.h"

namespace chrome_pdf {
namespace features {

BASE_DECLARE_FEATURE(kAccessiblePDFForm);
BASE_DECLARE_FEATURE(kPdfIncrementalLoading);
BASE_DECLARE_FEATURE(kPdfPartialLoading);
BASE_DECLARE_FEATURE(kPdfUseSkiaRenderer);
BASE_DECLARE_FEATURE(kPdfXfaSupport);

}  // namespace features
}  // namespace chrome_pdf

#endif  // PDF_PDF_FEATURES_H_
