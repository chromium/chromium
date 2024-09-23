// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/ca_layer_params.h"

namespace ui {
class AcceleratedWidgetMacNSView;
class RecyclableCompositorMac;
}

namespace content {

class BrowserCompositorMacClient {
 public:
  virtual SkColor BrowserCompositorMacGetGutterColor() const = 0;
  virtual void OnFrameTokenChanged(uint32_t frame_token,
                                   base::TimeTicks activation_time) = 0;
  virtual void DestroyCompositorForShutdown() = 0;
  virtual bool OnBrowserCompositorSurfaceIdChanged() = 0;
  virtual std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() = 0;
  virtual display::ScreenInfo GetCurrentScreenInfo() const = 0;
  virtual void SetCurrentDeviceScaleFactor(float device_scale_factor) = 0;
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
      const viz::FrameSinkId& frame_sink_id);
  ~BrowserCompositorMac() override;

  // These will not return nullptr until Destroy is called.
  DelegatedFrameHost* GetDelegatedFrameHost();

  // Force a new surface id to be allocated. Returns true if the
  // RenderWidgetHostImpl sent the resulting surface id to the renderer.
  bool ForceNewSurfaceId();

  // Return the parameters of the most recently received frame, or nullptr if
  // no valid frame is available.
  const gfx::CALayerParams* GetLastCALayerParams() const;

  void SetBackgroundColor(SkColor background_color);
  void TakeFallbackContentFrom(BrowserCompositorMac* other);

  // Update the renderer's SurfaceId to reflect the current dimensions of the
  // NSView. This will allocate a new SurfaceId, so should only be called
  // when necessary.
  void UpdateSurfaceFromNSView(const gfx::Size& new_size_dip);

  // Update the renderer's SurfaceId to reflect |new_size_in_pixels| in
  // anticipation of the NSView resizing during auto-resize.
  void UpdateSurfaceFromChild(
      bool auto_resize_enabled,
      float new_device_scale_factor,
      const gfx::Size& new_size_in_pixels,
      const viz::LocalSurfaceId& child_local_surface_id);

  // This is used to ensure that the ui::Compositor be attached to the
  // DelegatedFrameHost while the RWHImpl is visible.
  // Note: This should be called before the RWHImpl is made visible and after
  // it has been hidden, in order to ensure that thumbnailer notifications to
  // initiate copies occur before the ui::Compositor be detached.
  void SetRenderWidgetHostIsHidden(bool hidden);

  // Specify if the ui::Layer should be visible or not.
  void SetViewVisible(bool visible);

  // Sets or clears the parent ui::Layer and updates state to reflect that
  // we are now using the ui::Compositor from |parent_ui_layer| (if non-nullptr)
  // or one from |recyclable_compositor_| (if a compositor is needed).
  void SetParentUiLayer(ui::Layer* parent_ui_layer);

  viz::FrameSinkId GetRootFrameSinkId();

  const gfx::Size& GetRendererSize() const { return dfh_size_dip_; }
  viz::ScopedSurfaceIdAllocator GetScopedRendererSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task);
  const viz::LocalSurfaceId& GetRendererLocalSurfaceId();
  void TransformPointToRootSurface(gfx::PointF* point);

  // Indicate that the recyclable compositor should be destroyed, and no future
  // compositors should be recycled.
  static void DisableRecyclingForShutdown();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  float GetDeviceScaleFactor() const override;
  void InvalidateLocalSurfaceIdOnEviction() override;
  viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction() override;
  bool ShouldShowStaleContentOnEviction() override;

  base::WeakPtr<BrowserCompositorMac> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Dispatched when the page is being navigated to a different document. The
  // new page hasn't been marked as active yet.
  void DidNavigateMainFramePreCommit();

  // Dispatched after the old page has been unloaded and has entered the
  // `BackForwardCache`.
  void DidEnterBackForwardCache();

  void DidNavigate();

  void ForceNewSurfaceForTesting();

  ui::Compositor* GetCompositor() const;

  void InvalidateSurfaceAllocationGroup();

 private:
  // ui::LayerObserver implementation:
  void LayerDestroyed(ui::Layer* layer) override;

  cc::DeadlinePolicy GetDeadlinePolicy(bool is_resize) const;

  // The state of |delegated_frame_host_| and |recyclable_compositor_| to
  // manage being visible, hidden, or drawn via a ui::Layer.
  // The state of |recyclable_compositor_| and |parent_ui_layer_|.
  enum State {
    // We are drawing using |recyclable_compositor_|. This happens when the
    // renderer, but no parent ui::Layer has been specified. This is used by
    // content shell, popup windows (time/date picker), and when tab capturing
    // a backgrounded tab.
    HasOwnCompositor,
    // There is no compositor. This is true when the renderer is not visible
    // and no parent ui::Layer is specified.
    HasNoCompositor,
    // We are drawing using |parent_ui_layer_|'s compositor. This happens
    // whenever |parent_ui_layer_| is non-nullptr.
    UseParentLayerCompositor,
  };
  State state_ = HasNoCompositor;
  void UpdateState();
  void TransitionToState(State new_state);

  // Weak pointer to the layer supplied and reset via SetParentUiLayer. |this|
  // is an observer of |parent_ui_layer_|, to ensure that |parent_ui_layer_|
  // always be valid when non-null. The UpdateState function will re-parent
  // |root_layer_| to be under |parent_ui_layer_|, if needed.
  raw_ptr<ui::Layer> parent_ui_layer_ = nullptr;
  bool render_widget_host_is_hidden_ = true;

  raw_ptr<BrowserCompositorMacClient> client_ = nullptr;
  raw_ptr<ui::AcceleratedWidgetMacNSView> accelerated_widget_mac_ns_view_ =
      nullptr;
  std::unique_ptr<ui::RecyclableCompositorMac> recyclable_compositor_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<ui::Layer> root_layer_;

  SkColor background_color_ = SK_ColorWHITE;

  // The viz::ParentLocalSurfaceIdAllocator for the delegated frame host
  // dispenses viz::LocalSurfaceIds that are renderered into by the renderer
  // process.  These values are not updated during resize.
  viz::ParentLocalSurfaceIdAllocator dfh_local_surface_id_allocator_;
  gfx::Size dfh_size_pixels_;
  gfx::Size dfh_size_dip_;
  float dfh_device_scale_factor_ = 1.f;

  bool is_first_navigation_ = true;

  base::WeakPtrFactory<BrowserCompositorMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
