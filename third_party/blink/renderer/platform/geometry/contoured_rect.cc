// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

String ContouredRect::CornerCurvature::ToString() const {
  return String::Format("tl:%.2f; tr:%.2f; bl:%.2f; br:%.2f", TopLeft(),
                        TopRight(), BottomLeft(), BottomRight());
}

String ContouredRect::ToString() const {
  String rect_string = rect_.ToString();

  if (HasRoundCurvature()) {
    return rect_string;
  }

  return rect_string + " curvature:(" + GetCornerCurvature().ToString() + ")";
}

bool ContouredRect::IntersectsQuad(const gfx::QuadF& quad) const {
  return HasRoundCurvature() ? rect_.IntersectsQuad(quad)
                             : GetPath().Intersects(quad);
}

Path ContouredRect::GetPath() const {
  return Path::MakeContouredRect(*this);
}

// TODO(crbug.com/399449172) proper rendering of corner-shape with constant
// thickness
void ContouredRect::Outset(const gfx::OutsetsF& outsets) {
  rect_.Outset(outsets);
}

// TODO(crbug.com/397459628) support corner-shape for shadows/margins
void ContouredRect::OutsetForMarginOrShadow(const gfx::OutsetsF& outsets) {
  rect_.OutsetForMarginOrShadow(outsets);
}

}  // namespace blink
