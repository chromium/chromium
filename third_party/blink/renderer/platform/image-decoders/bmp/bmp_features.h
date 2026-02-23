// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_

#include "third_party/blink/public/common/features.h"

namespace blink {

using ::blink::features::kRustyBmpFeature;

inline bool IsRustyBmpEnabled() {
  return base::FeatureList::IsEnabled(kRustyBmpFeature);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_FEATURES_H_
