// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_CLAMPING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_CLAMPING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT CSSValueClampingUtils {
  STATIC_ONLY(CSSValueClampingUtils);

 public:
  static double ClampDouble(double value);
  static double ClampAngle(double value);
  static double ClampLength(double value);
  static double ClampTime(double value);
  static float ClampLength(float value);
  // TODO(crbug.com/1133390): The clamping functions for specific properties
  // (i.e. <angle> .. ) will be added.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_CLAMPING_UTILS_H_
