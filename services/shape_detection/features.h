// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FEATURES_H_
#define SERVICES_SHAPE_DETECTION_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Avoid a crash in Barhopper library when detecting Aztec barcodes.
// See crbug.com/442001297.
BASE_DECLARE_FEATURE(kBarhopperAztecRefineTransformFallback);

}  // namespace features

#endif  // SERVICES_SHAPE_DETECTION_FEATURES_H_
