// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/double_rect.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

DoubleRect::DoubleRect(const gfx::Rect& r)
    : location_(r.origin()), size_(r.size()) {}

DoubleRect::DoubleRect(const FloatRect& r)
    : location_(r.origin()), size_(r.size()) {}

DoubleRect::DoubleRect(const LayoutRect& r)
    : location_(r.Location()), size_(r.Size()) {}

gfx::Rect ToEnclosingRect(const DoubleRect& rect) {
  gfx::Point location = ToFlooredPoint(rect.MinXMinYCorner());
  gfx::Point max_point = ToCeiledPoint(rect.MaxXMaxYCorner());

  return gfx::Rect(location,
                   gfx::Size(base::ClampSub(max_point.x(), location.x()),
                             base::ClampSub(max_point.y(), location.y())));
}

gfx::Rect ToEnclosedRect(const DoubleRect& rect) {
  gfx::Point location = ToCeiledPoint(rect.MinXMinYCorner());
  gfx::Point max_point = ToFlooredPoint(rect.MaxXMaxYCorner());
  return gfx::BoundingRect(location, max_point);
}

gfx::Rect RoundedIntRect(const DoubleRect& rect) {
  return gfx::Rect(ToRoundedPoint(rect.Location()), ToRoundedSize(rect.Size()));
}

void DoubleRect::Scale(float sx, float sy) {
  location_.SetX(X() * sx);
  location_.SetY(Y() * sy);
  size_.SetWidth(Width() * sx);
  size_.SetHeight(Height() * sy);
}

std::ostream& operator<<(std::ostream& ostream, const DoubleRect& rect) {
  return ostream << rect.ToString();
}

String DoubleRect::ToString() const {
  return String::Format("%s %s", Location().ToString().Ascii().c_str(),
                        Size().ToString().Ascii().c_str());
}

}  // namespace blink
