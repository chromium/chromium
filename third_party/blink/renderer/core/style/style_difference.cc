// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_difference.h"

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
      NOTREACHED();
      break;
  }

  out << ", collectInlines=" << diff.needs_collect_inlines_;
  out << ", reshape=" << diff.needs_reshape_;

  out << ", paintInvalidationType=";
  switch (diff.paint_invalidation_type_) {
    case StyleDifference::kNoPaintInvalidation:
      out << "NoPaintInvalidation";
      break;
    case StyleDifference::kPaintInvalidationObject:
      out << "PaintInvalidationObject";
      break;
    case StyleDifference::kPaintInvalidationSubtree:
      out << "PaintInvalidationSubtree";
      break;
    default:
      NOTREACHED();
      break;
  }

  out << ", recomputeOverflow=" << diff.recompute_overflow_;
  out << ", visualRectUpdate=" << diff.visual_rect_update_;

  out << ", propertySpecificDifferences=";
  int diff_count = 0;
  for (int i = 0; i < StyleDifference::kPropertyDifferenceCount; i++) {
    unsigned bit_test = 1 << i;
    if (diff.property_specific_differences_ & bit_test) {
      if (diff_count++ > 0)
        out << "|";
      switch (bit_test) {
        case StyleDifference::kTransformChanged:
          out << "TransformChanged";
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
        case StyleDifference::kBackdropFilterChanged:
          out << "BackdropFilterChanged";
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
          NOTREACHED();
          break;
      }
    }
  }

  out << ", scrollAnchorDisablingPropertyChanged="
      << diff.scroll_anchor_disabling_property_changed_;

  return out << "}";
}

}  // namespace blink
