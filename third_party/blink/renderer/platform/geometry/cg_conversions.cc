// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/cg_conversions.h"

#include <ApplicationServices/ApplicationServices.h>

#include "ui/gfx/geometry/point.h"

namespace blink {

gfx::Point CGPointToPoint(const CGPoint& p) {
  return gfx::Point(static_cast<int>(p.x), static_cast<int>(p.y));
}

CGPoint PointToCGPoint(const gfx::Point& p) {
  return CGPointMake(p.x(), p.y());
}

}  // namespace blink
