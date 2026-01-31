// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kRustyBmpFeature);

inline bool IsRustyBmpEnabled() {
  return base::FeatureList::IsEnabled(kRustyBmpFeature);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_
