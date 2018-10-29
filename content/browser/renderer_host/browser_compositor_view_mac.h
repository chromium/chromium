// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/public/common/screen_info.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/display/display.h"
#include "ui/gfx/ca_layer_params.h"

namespace ui {
class AcceleratedWidgetMacNSView;
class RecyclableCompositorMac;
}

namespace content {

class BrowserCompositorMacClient {
 public:
  virtual SkColor BrowserCompositorMacGetGutterColor() const = 0;
  virtual void BrowserCompositorMacOnBeginFrame(base::TimeTicks frame_time) = 0;
  virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
  virtual void DestroyCompositorForShutdown() = 0;
  virtual bool SynchronizeVisualProperties(
      const base::Optional<viz::LocalSurfaceId>&
          child_allocated_local_surface_id,
      const base::Optional<base::TimeTicks>&
          child_local_surface_id_allocation_time) = 0;
};

// This class owns a DelegatedFrameHost, and will dynamically attach and
// detach it from a ui::Compositor as needed. The ui::Compositor will be
// detached from the DelegatedFrameHost when the following conditions are
// all met:
// - The RenderWidgetHostImpl providing frames to the DelegatedFrameHost
//   is visible.
// - The RenderWidgetHostViewMac that is used to display these frames is
//   attached to the NSView hierarchy of an NSWindow.
class CONTENT_EXPORT BrowserCompositorMac : public DelegatedFrameHostClient,
                                            public ui::LayerObserver {
 public:
  BrowserCompositorMac(
      ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view,
      BrowserCompositorMacClient* client,
      bool render_widget_host_is_hidden,
      const display::Display& initial_display,
      const viz::FrameSinkId& frame_sink_id);
  ~BrowserCompositorMac() override;

  // These will not return nullptr until Destroy is called.
  DelegatedFrameHost* GetDelegatedFrameHost();

  // Ensure that the currect compositor frame be cleared (even if it is
  // potentially visible).
  void ClearCompositorFrame();

  bool RequestRepaintForTesting();

  // Return the parameters of the most recently received frame, or nullptr if
  // no valid frame is available.
  const gfx::CALayerParams* GetLastCALayerParams() const;

  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink);
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack);
  void SetBackgroundColor(SkColor background_color);
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval);
  void SetNeedsBeginFrames(bool needs_begin_frames);
  void SetWantsAnimateOnlyBeginFrames();
  void TakeFallbackContentFrom(BrowserCompositorMac* other);

  // Update the renderer's SurfaceId to reflect the current dimensions of the
  // NSView. This will allocate a new SurfaceId if needed. This will return
  // true if any properties that need to be communicated to the
  // RenderWidgetHostImpl have changed.
  bool UpdateNSViewAndDisplay(const gfx::Size& new_size_dip,
                              const display::Display& new_display);

  // Update the renderer's SurfaceId to reflect |new_size_in_pixels| in
  // anticipation of the NSView resizing during auto-resize.
  void SynchronizeVisualProperties(
      float new_device_scale_factor,
      const gfx::Size& new_size_in_pixels,
      const viz::LocalSurfaceId& child_allocated_local_surface_id,
      base::TimeTicks child_local_surface_id_allocation_time);

  // This is used to ensure that the ui::Compositor be attached to the
  // DelegatedFrameHost while the RWHImpl is visible.
  // Note: This should be called before the RWHImpl is made visible and after
  // it has been hidden, in order to ensure that thumbnailer notifications to
  // initiate copies occur before the ui::Compositor be detached.
  void SetRenderWidgetHostIsHidden(bool hidden);

  // This is used to ensure that the ui::Compositor be attached to this
  // NSView while its contents may be visible on-screen, even if the RWHImpl is
  // hidden (e.g, because it is occluded by another window).
  void SetNSViewAttachedToWindow(bool attached);

  // Specify if the ui::Layer should be visible or not.
  void SetViewVisible(bool visible);

  // Sets or clears the parent ui::Layer and updates state to reflect that
  // we are now using the ui::Compositor from |parent_ui_layer| (if non-nullptr)
  // or one from |recyclable_compositor_| (if a compositor is needed).
  void SetParentUiLayer(ui::Layer* parent_ui_layer);

  viz::FrameSinkId GetRootFrameSinkId();

  const gfx::Size& GetRendererSize() const { return dfh_size_dip_; }
  void GetRendererScreenInfo(ScreenInfo* screen_info) const;
  viz::ScopedSurfaceIdAllocator GetScopedRendererSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task);
  const viz::LocalSurfaceId& GetRendererLocalSurfaceId();
  base::TimeTicks GetRendererLocalSurfaceIdAllocationTime() const;
  const viz::LocalSurfaceId& AllocateNewRendererLocalSurfaceId();
  bool UpdateRendererLocalSurfaceIdFromChild(
      const viz::LocalSurfaceId& child_allocated_local_surface_id,
      base::TimeTicks child_local_surface_id_allocation_time);
  void TransformPointToRootSurface(gfx::PointF* point);

  // Indicate that the recyclable compositor should be destroyed, and no future
  // compositors should be recycled.
  static void DisableRecyclingForShutdown();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  void OnBeginFrame(base::TimeTicks frame_time) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  float GetDeviceScaleFactor() const override;
  void WasEvicted() override;

  base::WeakPtr<BrowserCompositorMac> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void DidNavigate();

  bool ShouldContinueToPauseForFrame() const;

  bool ForceNewSurfaceForTesting();

  ui::Compositor* GetCompositor() const;

 private:
  // ui::LayerObserver implementation:
  void LayerDestroyed(ui::Layer* layer) override;

  cc::DeadlinePolicy GetDeadlinePolicy(bool is_resize) const;

  // The state of |delegated_frame_host_| and |recyclable_compositor_| to
  // manage being visible, hidden, or drawn via a ui::Layer.
  enum State {
    // Effects:
    // - |recyclable_compositor_| exists and is attached to
    //   |delegated_frame_host_|.
    // Happens when:
    // - |render_widet_host_| is in the visible state.
    HasAttachedCompositor,
    // Effects:
    // - |recyclable_compositor_| has been recycled and |delegated_frame_host_|
    //   is hidden and detached from it.
    // Happens when:
    // - The |render_widget_host_| hidden or gone, and |cocoa_view_| is not
    //   attached to an NSWindow.
    // - This happens for backgrounded tabs.
    HasNoCompositor,
    // Effects:
    // - |recyclable_compositor_| does not exist. |delegated_frame_host_| is
    //   attached to |parent_ui_layer_|'s compositor.
    // Happens when:
    // - |parent_ui_layer_| is non-nullptr.
    UseParentLayerCompositor,
  };
  State state_ = HasNoCompositor;
  void UpdateState();
  void TransitionToState(State new_state);

  // Weak pointer to the layer supplied and reset via SetParentUiLayer. |this|
  // is an observer of |parent_ui_layer_|, to ensure that |parent_ui_layer_|
  // always be valid when non-null. The UpdateState function will re-parent
  // |root_layer_| to be under |parent_ui_layer_|, if needed.
  ui::Layer* parent_ui_layer_ = nullptr;
  bool render_widget_host_is_hidden_ = true;
  bool ns_view_attached_to_window_ = false;

  BrowserCompositorMacClient* client_ = nullptr;
  ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view_ = nullptr;
  std::unique_ptr<ui::RecyclableCompositorMac> recyclable_compositor_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<ui::Layer> root_layer_;

  SkColor background_color_ = SK_ColorWHITE;
  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  // The viz::ParentLocalSurfaceIdAllocator for the delegated frame host
  // dispenses viz::LocalSurfaceIds that are renderered into by the renderer
  // process.
  viz::ParentLocalSurfaceIdAllocator dfh_local_surface_id_allocator_;
  gfx::Size dfh_size_pixels_;
  gfx::Size dfh_size_dip_;
  display::Display dfh_display_;

  bool is_first_navigation_ = true;

  base::WeakPtrFactory<BrowserCompositorMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
