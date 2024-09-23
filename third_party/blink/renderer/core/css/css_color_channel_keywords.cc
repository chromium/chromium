// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_channel_keywords.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/geometry/color_channel_keyword.h"

namespace blink {

ColorChannelKeyword CSSValueIDToColorChannelKeyword(CSSValueID value) {
  switch (value) {
    case CSSValueID::kA:
      return ColorChannelKeyword::kA;
    case CSSValueID::kB:
      return ColorChannelKeyword::kB;
    case CSSValueID::kC:
      return ColorChannelKeyword::kC;
    case CSSValueID::kG:
      return ColorChannelKeyword::kG;
    case CSSValueID::kH:
      return ColorChannelKeyword::kH;
    case CSSValueID::kL:
      return ColorChannelKeyword::kL;
    case CSSValueID::kR:
      return ColorChannelKeyword::kR;
    case CSSValueID::kS:
      return ColorChannelKeyword::kS;
    case CSSValueID::kW:
      return ColorChannelKeyword::kW;
    case CSSValueID::kX:
      return ColorChannelKeyword::kX;
    case CSSValueID::kY:
      return ColorChannelKeyword::kY;
    case CSSValueID::kZ:
      return ColorChannelKeyword::kZ;
    case CSSValueID::kAlpha:
      return ColorChannelKeyword::kAlpha;
    default:
      NOTREACHED();
  }
}

CSSValueID ColorChannelKeywordToCSSValueID(ColorChannelKeyword keyword) {
  switch (keyword) {
    case ColorChannelKeyword::kA:
      return CSSValueID::kA;
    case ColorChannelKeyword::kB:
      return CSSValueID::kB;
    case ColorChannelKeyword::kC:
      return CSSValueID::kC;
    case ColorChannelKeyword::kG:
      return CSSValueID::kG;
    case ColorChannelKeyword::kH:
      return CSSValueID::kH;
    case ColorChannelKeyword::kL:
      return CSSValueID::kL;
    case ColorChannelKeyword::kR:
      return CSSValueID::kR;
    case ColorChannelKeyword::kS:
      return CSSValueID::kS;
    case ColorChannelKeyword::kW:
      return CSSValueID::kW;
    case ColorChannelKeyword::kX:
      return CSSValueID::kX;
    case ColorChannelKeyword::kY:
      return CSSValueID::kY;
    case ColorChannelKeyword::kZ:
      return CSSValueID::kZ;
    case ColorChannelKeyword::kAlpha:
      return CSSValueID::kAlpha;
  }
}

}  // namespace blink
