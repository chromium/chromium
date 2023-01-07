// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dip_util.h"

#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/layout.h"
#include "ui/display/display_util.h"
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
  display::ScreenInfo screen_info;
  display::DisplayUtil::GetDefaultScreenInfo(&screen_info);
  return screen_info.device_scale_factor;
}

}  // namespace content
