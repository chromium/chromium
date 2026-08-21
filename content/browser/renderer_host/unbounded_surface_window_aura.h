// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_AURA_H_

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/unbounded_element/unbounded_element.mojom.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/events/gestures/motion_event_aura.h"

namespace ui {
class Layer;
}

namespace content {

class RenderWidgetHostViewAura;
class RenderWidgetHostViewBase;

class UnboundedSurfaceWindowAura : public UnboundedSurfaceWindow,
                                   public aura::WindowDelegate,
                                   public aura::WindowObserver,
                                   public viz::HostFrameSinkClient {
 public:
  static std::unique_ptr<UnboundedSurfaceWindowAura> Create(
      RenderWidgetHostViewAura* parent_view,
      mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
      mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient>
          client,
      const gfx::Rect& bounds_in_screen,
      base::WeakPtr<RenderWidgetHostViewBase> subframe_view);

  ~UnboundedSurfaceWindowAura() override;

  // UnboundedSurfaceWindow overrides:
  base::WeakPtr<UnboundedSurfaceWindow> GetWeakPtr() override;
  bool IsValid() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  void SetBounds(const gfx::Rect& bounds_in_screen) override;
  viz::FrameSinkId GetFrameSinkId() const override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client)
      override;

  void RouteMouseEvent(const blink::WebMouseEvent& event) override;
  gfx::Rect GetBounds() const override;
  void CopyFromSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      base::TimeDelta timeout,
      base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback)
      override;
  void EnsureSurfaceSynchronizedForWebTest() override;

  // blink::mojom::UnboundedSurfaceHost overrides:
  void UpdateBounds(const gfx::Rect& bounds) override;

  // aura::WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override;
  std::optional<gfx::Size> GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override {}

  // ui::EventHandler overrides (from WindowDelegate):
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // viz::HostFrameSinkClient overrides:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
  }
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

 protected:
  void TeardownAndDestroy() override;

 private:
  class DebugBorderDelegate;

  UnboundedSurfaceWindowAura(
      RenderWidgetHostViewAura* parent_view,
      mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
      mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient>
          client,
      base::WeakPtr<RenderWidgetHostViewBase> subframe_view);
  bool InitWindow(const gfx::Rect& bounds_in_screen);
  void OnConnectionError();

  raw_ptr<RenderWidgetHostViewAura> parent_view_;
  base::WeakPtr<RenderWidgetHostViewBase> subframe_view_;
  viz::FrameSinkId frame_sink_id_;
  viz::FrameSinkId parent_frame_sink_id_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  mojo::AssociatedReceiver<blink::mojom::UnboundedSurfaceHost> receiver_{this};
  std::unique_ptr<aura::Window> window_;
  raw_ptr<aura::Window> root_window_ = nullptr;
  ui::MotionEventAura pointer_state_;
  std::unique_ptr<DebugBorderDelegate> debug_border_delegate_;
  std::unique_ptr<ui::Layer> debug_border_layer_;
  base::WeakPtrFactory<UnboundedSurfaceWindow> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_AURA_H_
