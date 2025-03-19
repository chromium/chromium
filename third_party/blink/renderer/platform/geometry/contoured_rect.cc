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

}  // namespace blink
