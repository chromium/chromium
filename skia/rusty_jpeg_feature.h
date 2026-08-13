// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_RUSTY_JPEG_FEATURE_H_
#define SKIA_RUSTY_JPEG_FEATURE_H_

#include "base/feature_list.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

// Exposing the feature so that chrome://flags and tests can inspect it and turn
// it on/off, but product code should instead just call `IsRustyJpegEnabled`.
SK_API BASE_DECLARE_FEATURE(kRustyJpegFeature);

// Returns true if Rust should be used for JPEG decoding and encoding.
inline bool IsRustyJpegEnabled() {
  return base::FeatureList::IsEnabled(kRustyJpegFeature);
}

}  // namespace skia

#endif  // SKIA_RUSTY_JPEG_FEATURE_H_
