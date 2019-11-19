// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dip_util.h"

#include "content/browser/renderer_host/display_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

float GetScaleFactorForView(RenderWidgetHostView* view) {
  if (view)
    return view->GetDeviceScaleFactor();
  ScreenInfo screen_info;
  DisplayUtil::GetDefaultScreenInfo(&screen_info);
  return screen_info.device_scale_factor;
}

gfx::Point ConvertViewPointToDIP(RenderWidgetHostView* view,
                                 const gfx::Point& point_in_pixel) {
  return gfx::ConvertPointToDIP(GetScaleFactorForView(view), point_in_pixel);
}

gfx::Size ConvertViewSizeToPixel(RenderWidgetHostView* view,
                                 const gfx::Size& size_in_dip) {
  return gfx::ConvertSizeToPixel(GetScaleFactorForView(view), size_in_dip);
}

gfx::Rect ConvertViewRectToPixel(RenderWidgetHostView* view,
                                 const gfx::Rect& rect_in_dip) {
  return gfx::ConvertRectToPixel(GetScaleFactorForView(view), rect_in_dip);
}

}  // namespace content
