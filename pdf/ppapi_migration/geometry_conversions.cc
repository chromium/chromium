// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/geometry_conversions.h"

#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

gfx::Point PointFromPPPoint(const PP_Point& pp_point) {
  return gfx::Point(pp_point.x, pp_point.y);
}

gfx::PointF PointFFromPPFloatPoint(const PP_FloatPoint& pp_point) {
  return gfx::PointF(pp_point.x, pp_point.y);
}

gfx::Rect RectFromPPRect(const PP_Rect& pp_rect) {
  return gfx::Rect(pp_rect.point.x, pp_rect.point.y, pp_rect.size.width,
                   pp_rect.size.height);
}

PP_Rect PPRectFromRect(const gfx::Rect& rect) {
  return PP_MakeRectFromXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

PP_FloatRect PPFloatRectFromRectF(const gfx::RectF& rect) {
  return PP_MakeFloatRectFromXYWH(rect.x(), rect.y(), rect.width(),
                                  rect.height());
}

gfx::Size SizeFromPPSize(const PP_Size& pp_size) {
  return gfx::Size(pp_size.width, pp_size.height);
}

PP_Size PPSizeFromSize(const gfx::Size& size) {
  return PP_MakeSize(size.width(), size.height());
}

}  // namespace chrome_pdf
