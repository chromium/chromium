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
      out << "None";
      break;
    case StyleDifference::kPositionedLayout:
      out << "Positioned";
      break;
    case StyleDifference::kFullLayout:
      out << "Full";
      break;
    default:
      NOTREACHED();
  }

  out << ", reshape=" << diff.needs_reshape;

  out << ", paintInvalidationType=";
  switch (diff.paint_type_) {
    case StyleDifference::kNoPaint:
      out << "None";
      break;
    case StyleDifference::kSimplePaint:
      out << "Simple";
      break;
    case StyleDifference::kNormalPaint:
      out << "Normal";
      break;
    default:
      NOTREACHED();
  }

  out << ", recomputeVisualOverflow=" << diff.needs_recompute_visual_overflow;

  if (diff.blend_mode_changed) {
    out << ", BlendModeChanged";
  }
  if (diff.clip_property_changed) {
    out << ", ClipPropertyChanged";
  }
  if (diff.filter_changed) {
    out << ", FilterChanged";
  }
  if (diff.opacity_changed) {
    out << ", OpacityChanged";
  }
  if (diff.only_transform_property_changed) {
    out << ", OnlyTransformPropertyChanged";
  }
  if (diff.text_decoration_or_color_changed) {
    out << ", TextDecorationOrColorChanged";
  }
  if (diff.transform_changed) {
    out << ", TransformChanged";
  }
  if (diff.z_index_changed) {
    out << ", ZIndexChanged";
  }

  out << ", disableScrollAnchoring=" << diff.disable_scroll_anchoring;

  return out << "}";
}

}  // namespace blink
