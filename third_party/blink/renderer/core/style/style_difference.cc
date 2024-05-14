// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_difference.h"

#include <ostream>

#include "base/notreached.h"

namespace blink {

std::ostream& operator<<(std::ostream& out, const StyleDifference& diff) {
  out << "StyleDifference{layoutType=";

  switch (diff.layout_type_) {
    case StyleDifference::kNoLayout:
      out << "NoLayout";
      break;
    case StyleDifference::kPositionedMovement:
      out << "PositionedMovement";
      break;
    case StyleDifference::kFullLayout:
      out << "FullLayout";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  out << ", reshape=" << diff.needs_reshape_;

  out << ", paintInvalidationType=";
  switch (diff.paint_invalidation_type_) {
    case static_cast<unsigned>(StyleDifference::PaintInvalidationType::kNone):
      out << "None";
      break;
    case static_cast<unsigned>(StyleDifference::PaintInvalidationType::kSimple):
      out << "Simple";
      break;
    case static_cast<unsigned>(StyleDifference::PaintInvalidationType::kNormal):
      out << "Normal";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  out << ", recomputeVisualOverflow=" << diff.recompute_visual_overflow_;

  out << ", propertySpecificDifferences=";
  int diff_count = 0;
  for (int i = 0; i < StyleDifference::kPropertyDifferenceCount; i++) {
    unsigned bit_test = 1 << i;
    if (diff.property_specific_differences_ & bit_test) {
      if (diff_count++ > 0) {
        out << "|";
      }
      switch (bit_test) {
        case StyleDifference::kTransformPropertyChanged:
          out << "TransformPropertyChanged";
          break;
        case StyleDifference::kOtherTransformPropertyChanged:
          out << "OtherTransformPropertyChanged";
          break;
        case StyleDifference::kOpacityChanged:
          out << "OpacityChanged";
          break;
        case StyleDifference::kZIndexChanged:
          out << "ZIndexChanged";
          break;
        case StyleDifference::kFilterChanged:
          out << "FilterChanged";
          break;
        case StyleDifference::kCSSClipChanged:
          out << "CSSClipChanged";
          break;
        case StyleDifference::kTextDecorationOrColorChanged:
          out << "TextDecorationOrColorChanged";
          break;
        case StyleDifference::kBlendModeChanged:
          out << "BlendModeChanged";
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }

  out << ", scrollAnchorDisablingPropertyChanged="
      << diff.scroll_anchor_disabling_property_changed_;

  return out << "}";
}

}  // namespace blink
