// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_MAC_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/gfx/display_color_spaces.h"

#ifdef __OBJC__
@class NSWindow;
@class NSEvent;
#else
class NSWindow;
class NSEvent;
#endif

#include "components/viz/host/host_frame_sink_client.h"

namespace ui {
class RecyclableCompositorMac;
class DisplayCALayerTree;
}  // namespace ui

namespace content {

class RenderFrameHostImpl;

class UnboundedSurfaceWindowMac : public UnboundedSurfaceWindow,
                                  public viz::HostFrameSinkClient,
                                  public ui::AcceleratedWidgetMacNSView {
 public:
  UnboundedSurfaceWindowMac(RenderFrameHostImpl* parent_rfh,
                            const gfx::Rect& bounds_in_screen);
  ~UnboundedSurfaceWindowMac() override;

  // UnboundedSurfaceWindow overrides:
  bool is_valid() const override;
  void SetBounds(const gfx::Rect& bounds_in_screen) override;
  viz::FrameSinkId GetFrameSinkId() const override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client)
      override;

  gfx::Rect GetBoundsForTesting() const override;

  // Event Routing:
  void RouteMouseEvent(NSEvent* event);
  void RouteMouseEvent(const blink::WebMouseEvent& event) override;
  void RouteKeyboardEvent(NSEvent* event);

  // Resign Key:
  void OnWindowResignedKey();

  // viz::HostFrameSinkClient overrides:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
  }
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

  // ui::AcceleratedWidgetMacNSView overrides:
  void AcceleratedWidgetCALayerParamsUpdated(
      gfx::CALayerParams ca_layer_params) override;

 private:
  struct DisplayInfo {
    float scale_factor = 1.0f;
    gfx::DisplayColorSpaces display_color_spaces;
    int64_t display_id = display::kInvalidDisplayId;
  };

  DisplayInfo GetDisplayInfo() const;
  void InitWindow(const gfx::Rect& bounds_in_screen);

  raw_ptr<RenderFrameHostImpl> parent_rfh_;
  viz::FrameSinkId frame_sink_id_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
#ifdef __OBJC__
  NSWindow* __strong window_ = nil;
#else
  raw_ptr<void> window_ = nullptr;
#endif

  std::unique_ptr<ui::RecyclableCompositorMac> recyclable_compositor_;
  std::unique_ptr<ui::Layer> root_layer_;
  std::unique_ptr<ui::DisplayCALayerTree> display_ca_layer_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_MAC_H_
