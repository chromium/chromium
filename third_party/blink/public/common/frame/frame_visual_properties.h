// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_

#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// TODO(fsamuel): We might want to unify this with content::ResizeParams.
struct BLINK_COMMON_EXPORT FrameVisualProperties {
  FrameVisualProperties();
  FrameVisualProperties(const FrameVisualProperties& other);
  ~FrameVisualProperties();

  FrameVisualProperties& operator=(const FrameVisualProperties& other);

  // These fields are values from VisualProperties, see comments there for
  // descriptions. They exist here to propagate from each RenderWidget to its
  // child RenderWidgets. Here they are flowing from RenderWidget in a parent
  // renderer process up to the RenderWidgetHost for a child RenderWidget in
  // another renderer process. That RenderWidgetHost would then be responsible
  // for passing it along to the child RenderWidget.
  blink::ScreenInfo screen_info;
  bool auto_resize_enabled = false;
  bool is_pinch_gesture_active = false;
  uint32_t capture_sequence_number = 0u;
  double zoom_level = 0;
  float page_scale_factor = 1.f;
  gfx::Size visible_viewport_size;
  gfx::Size min_size_for_auto_resize;
  gfx::Size max_size_for_auto_resize;
  std::vector<gfx::Rect> root_widget_window_segments;

  // The size of the compositor viewport, to match the sub-frame's surface.
  gfx::Rect compositor_viewport;

  gfx::Rect screen_space_rect;
  gfx::Size local_frame_size;

  // The time at which the viz::LocalSurfaceId used to submit this was
  // allocated.
  viz::LocalSurfaceIdAllocation local_surface_id_allocation;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_
