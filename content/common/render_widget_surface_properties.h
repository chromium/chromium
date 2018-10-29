// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RENDER_WIDGET_SURFACE_PROPERTIES_H_
#define CONTENT_COMMON_RENDER_WIDGET_SURFACE_PROPERTIES_H_

#include "components/viz/common/quads/compositor_frame.h"
#include "content/common/content_export.h"

namespace content {

// This struct contains the properties that are constant among all
// CompositorFrames that the renderer submits to the same surface.
struct CONTENT_EXPORT RenderWidgetSurfaceProperties {
  static RenderWidgetSurfaceProperties FromCompositorFrame(
      const viz::CompositorFrame& frame);

  RenderWidgetSurfaceProperties();
  RenderWidgetSurfaceProperties(const RenderWidgetSurfaceProperties& other);
  ~RenderWidgetSurfaceProperties();

  RenderWidgetSurfaceProperties& operator=(
      const RenderWidgetSurfaceProperties& other);

  bool operator==(const RenderWidgetSurfaceProperties& other) const;
  bool operator!=(const RenderWidgetSurfaceProperties& other) const;

  std::string ToDiffString(const RenderWidgetSurfaceProperties& other) const;

  gfx::Size size;
  float device_scale_factor = 0;
  float top_controls_height = 0;
  float top_controls_shown_ratio = 0;
#ifdef OS_ANDROID
  float bottom_controls_height = 0;
  float bottom_controls_shown_ratio = 0;
  viz::Selection<gfx::SelectionBound> selection;
  bool has_transparent_background = false;
#endif
};

}  // namespace content

#endif  // CONTENT_COMMON_RENDER_WIDGET_SURFACE_PROPERTIES_H_
