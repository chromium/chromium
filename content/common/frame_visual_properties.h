// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_VISUAL_PROPERTIES_H_
#define CONTENT_COMMON_FRAME_VISUAL_PROPERTIES_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "content/common/content_export.h"
#include "content/public/common/screen_info.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// TODO(fsamuel): We might want to unify this with content::ResizeParams.
struct CONTENT_EXPORT FrameVisualProperties {
  FrameVisualProperties();
  FrameVisualProperties(const FrameVisualProperties& other);
  ~FrameVisualProperties();

  FrameVisualProperties& operator=(const FrameVisualProperties& other);

  // These fields are values from VisualProperties, see comments there for
  // descriptions. They exist here to propagate from each RenderWidget to its
  // child RenderWidgets. Here they flow back from RenderWidget to the host
  // in order to find a child RenderWidget.
  ScreenInfo screen_info;
  gfx::Size visible_viewport_size;
  bool auto_resize_enabled = false;
  gfx::Size min_size_for_auto_resize;
  gfx::Size max_size_for_auto_resize;
  uint32_t capture_sequence_number = 0u;
  double zoom_level = 0;
  float page_scale_factor = 1.f;
  bool is_pinch_gesture_active = false;

  // The size of the compositor viewport, to match the sub-frame's surface.
  gfx::Rect compositor_viewport;

  gfx::Rect screen_space_rect;
  gfx::Size local_frame_size;

  // The time at which the viz::LocalSurfaceId used to submit this was
  // allocated.
  viz::LocalSurfaceIdAllocation local_surface_id_allocation;
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_VISUAL_PROPERTIES_H_
