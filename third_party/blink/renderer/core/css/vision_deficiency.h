// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VISION_DEFICIENCY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VISION_DEFICIENCY_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class VisionDeficiency {
  kNoVisionDeficiency,
  kAchromatopsia,
  kBlurredVision,
  kDeuteranopia,
  kProtanopia,
  kTritanopia,
};

AtomicString CreateVisionDeficiencyFilterUrl(
    VisionDeficiency vision_deficiency);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VISION_DEFICIENCY_H_
