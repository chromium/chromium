// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_IOS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
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

namespace content {

class BrowserCompositorIOSClient {
 public:
  virtual SkColor BrowserCompositorIOSGetGutterColor() = 0;
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
// - The RenderWidgetHostViewIOS that is used to display these frames is
//   attached to the UIView hierarchy of an UIWindow.
class CONTENT_EXPORT BrowserCompositorIOS : public DelegatedFrameHostClient,
                                            public ui::LayerObserver {
 public:
  BrowserCompositorIOS(gfx::AcceleratedWidget accelerated_widget,
                       BrowserCompositorIOSClient* client,
                       bool render_widget_host_is_hidden,
                       const viz::FrameSinkId& frame_sink_id);
  ~BrowserCompositorIOS() override;

  // These will not return nullptr until Destroy is called.
  DelegatedFrameHost* GetDelegatedFrameHost();

  // Force a new surface id to be allocated. Returns true if the
  // RenderWidgetHostImpl sent the resulting surface id to the renderer.
  bool ForceNewSurfaceId();

  void SetBackgroundColor(SkColor background_color);
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval);
  void TakeFallbackContentFrom(BrowserCompositorIOS* other);

  // Update the renderer's SurfaceId to reflect the current dimensions of the
  // UIView. This will allocate a new SurfaceId, so should only be called
  // when necessary.
  void UpdateSurfaceFromUIView(const gfx::Size& new_size_dip);

  // Update the renderer's SurfaceId to reflect |new_size_in_pixels| in
  // anticipation of the UIView resizing during auto-resize.
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

  base::WeakPtr<BrowserCompositorIOS> GetWeakPtr() {
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

  void UpdateSurface(const gfx::Size& size_pixels,
                     float scale_factor,
                     const gfx::DisplayColorSpaces& display_color_spaces);

  void InvalidateSurface();
  void Suspend();
  void Unsuspend();

  // Weak pointer to the layer supplied and reset via SetParentUiLayer. |this|
  // is an observer of |parent_ui_layer_|, to ensure that |parent_ui_layer_|
  // always be valid when non-null. The UpdateState function will re-parent
  // |root_layer_| to be under |parent_ui_layer_|, if needed.
  raw_ptr<ui::Layer> parent_ui_layer_ = nullptr;
  bool render_widget_host_is_hidden_ = true;

  raw_ptr<BrowserCompositorIOSClient> client_;
  gfx::AcceleratedWidget accelerated_widget_;
  std::unique_ptr<ui::Compositor> compositor_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<ui::Layer> root_layer_;

  SkColor background_color_ = SK_ColorRED;

  // The viz::ParentLocalSurfaceIdAllocator for the delegated frame host
  // dispenses viz::LocalSurfaceIds that are rendered into by the renderer
  // process.  These values are not updated during resize.
  viz::ParentLocalSurfaceIdAllocator dfh_local_surface_id_allocator_;
  gfx::Size dfh_size_pixels_;
  gfx::Size dfh_size_dip_;
  float dfh_device_scale_factor_ = 1.f;

  bool is_first_navigation_ = true;

  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  gfx::Size size_pixels_;
  float scale_factor_ = 1.f;
  gfx::DisplayColorSpaces display_color_spaces_;
  std::unique_ptr<ui::CompositorLock> compositor_suspended_lock_;

  base::WeakPtrFactory<BrowserCompositorIOS> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_IOS_H_
