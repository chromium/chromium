// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_VISUAL_PROPERTIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_VISUAL_PROPERTIES_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/trees/browser_controls_params.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// Visual properties contain context required to render a frame tree.
// For legacy reasons, both Page visual properties [shared by all Renderers] and
// Widget visual properties [unique to local frame roots] are passed along the
// same data structure. Separating these is tricky because they both affect
// rendering, and if updates are received asynchronously, this can cause
// incorrect behavior.
// Visual properties are also used for Pepper fullscreen and popups, which are
// also based on Widgets.
//
// The data flow for VisualProperties is tricky. For legacy reasons, visual
// properties are currently always sent from RenderWidgetHosts to RenderWidgets.
// However, RenderWidgets can also send visual properties to out-of-process
// subframes [by bouncing through CrossProcessFrameConnector]. This causes a
// cascading series of VisualProperty messages. This is necessary due to the
// current implementation to make sure that cross-process surfaces get
// simultaneously synchronized. For more details, see:
// https://docs.google.com/document/d/1VKOLBYlujcn862w9LAyUbv6oW9RZgD65oDCI_G5AEVQ/edit#heading=h.wno2seszsyen
// https://docs.google.com/document/d/1J7BTRsylGApm6KHaaTu-m6LLvSWJgf1B9CM-USKIp1k/edit#heading=h.ichmoicfam1y
//
// Known problems:
// + It's not clear which properties are page-specific and which are
// widget-specific. We should document them.
// + It's not clear which properties are only set by the browser, which are only
// set by the renderer, and which are set by both.
// + Given the frame tree A(B(A')) where A and A' are same-origin, same process
// and B is separate origin separate process:
//   (1) RenderWidget A gets SynchronizeVisualProperties, passes it to proxy for
//   B, sets values on RenderView/Page.
//   (2) RenderWidget B gets SynchronizeVisualProperties, passes it to proxy for
//   A'
//   (3) RenderWidget A' gets SynchronizeVisualProperties.
// In between (1) and (3), frames associated with RenderWidget A' will see
// updated page properties from (1) but are still seeing old widget properties.

struct VisualProperties {
  // Information about the screen (dpi, depth, etc..).
  ScreenInfo screen_info;

  // Whether or not blink should be in auto-resize mode.
  bool auto_resize_enabled = false;

  // The minimum size for Blink if auto-resize is enabled.
  gfx::Size min_size_for_auto_resize;

  // The maximum size for Blink if auto-resize is enabled.
  gfx::Size max_size_for_auto_resize;

  // The size for the widget in DIPs.
  gfx::Size new_size;

  // The size of the area of the widget that is visible to the user, in DIPs.
  // The visible area may be empty if the visible area does not intersect with
  // the widget, for example in the case of a child frame that is entirely
  // scrolled out of the main frame's viewport. It may also be smaller than the
  // widget's size in |new_size| due to the UI hiding part of the widget, such
  // as with an on-screen keyboard.
  gfx::Size visible_viewport_size;

  // The rect of compositor's viewport in pixels. Note that for top level
  // widgets this is roughly the DSF scaled new_size put into a rect. For child
  // frame widgets it is a pixel-perfect bounds of the visible region of the
  // widget. The size would be similar to visible_viewport_size, but in physical
  // pixels and computed via very different means.
  // TODO(danakj): It would be super nice to remove one of |new_size|,
  // |visible_viewport_size| and |compositor_viewport_pixel_rect|. Their values
  // overlap in purpose, creating a very confusing situation about which to use
  // for what, and how they should relate or not.
  gfx::Rect compositor_viewport_pixel_rect;

  // Browser controls params such as top and bottom controls heights, whether
  // controls shrink blink size etc.
  cc::BrowserControlsParams browser_controls_params;

  // Whether or not the focused node should be scrolled into view after the
  // resize.
  bool scroll_focused_node_into_view = false;

  // The local surface ID to use (if valid).
  base::Optional<viz::LocalSurfaceId> local_surface_id;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted = false;

  // The display mode.
  mojom::DisplayMode display_mode = mojom::DisplayMode::kUndefined;

  // This represents the latest capture sequence number requested. When this is
  // incremented, that means the caller wants to synchronize surfaces which
  // should cause a new LocalSurfaceId to be generated.
  uint32_t capture_sequence_number = 0u;

  // This represents the page zoom level for a WebContents.
  // (0 is the default value which results in 1.0 zoom factor).
  double zoom_level = 0;

  // This represents the page's scale factor, which changes during pinch zoom.
  // It needs to be shared with subframes.
  float page_scale_factor = 1.f;

  // The logical segments of the root widget, in widget-relative DIPs. This
  // property is set by the root RenderWidget in the renderer process, then
  // propagated to child local frame roots via RenderFrameProxy/
  // CrossProcessFrameConnector.
  std::vector<gfx::Rect> root_widget_window_segments;

  // Indicates whether a pinch gesture is currently active. Originates in the
  // main frame's renderer, and needs to be shared with subframes.
  bool is_pinch_gesture_active = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_VISUAL_PROPERTIES_H_
