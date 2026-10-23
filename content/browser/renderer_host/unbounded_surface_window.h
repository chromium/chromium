// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace content {

class RenderWidgetHostViewBase;

// The native window displaying an unbounded element. This is a platform object
// owned by the root RenderWidgetHostView. The Mojo endpoints and the policy
// (bounds constraints, dismissal handshake) live in UnboundedSurfaceHostImpl,
// which is tied to the requesting document.
class CONTENT_EXPORT UnboundedSurfaceWindow {
 public:
  class Delegate {
   public:
    // Called when the user asks to dismiss the window (e.g. Esc, outside
    // click).
    virtual void OnDismissRequested() = 0;

    // Called when the window is destroyed for a reason other than the
    // delegate asking for it (e.g. another document opened a surface in the
    // same tab, or the root view went away). The window must not be used
    // anymore.
    virtual void OnWindowDestroyed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  UnboundedSurfaceWindow(base::WeakPtr<RenderWidgetHostViewBase> parent_view,
                         base::WeakPtr<RenderWidgetHostViewBase> subframe_view);
  virtual ~UnboundedSurfaceWindow();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  virtual bool IsValid() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  RenderWidgetHostViewBase* GetTargetView() const;

  // Initiates the dismissal of this window. The delegate runs the dismissal
  // handshake with the renderer. Without a delegate, the window is destroyed.
  void Dismiss();

  virtual void SetBounds(const gfx::Rect& bounds_in_screen) = 0;
  virtual viz::FrameSinkId GetFrameSinkId() const = 0;
  virtual viz::LocalSurfaceId GetLocalSurfaceId() const = 0;

  // Transforms a point in screen coordinates into the coordinate space of
  // `target_view`. Returns std::nullopt if the root view cannot be determined.
  static std::optional<gfx::PointF> TransformScreenPointToViewCoordSpace(
      RenderWidgetHostViewBase* target_view,
      const gfx::PointF& screen_point);

  // Transforms `web_event`'s screen position into `target_view`'s coordinate
  // space and updates its widget position. Returns true on success.
  static bool TransformEventCoordinatesToView(
      RenderWidgetHostViewBase* target_view,
      blink::WebMouseEvent& web_event);

  virtual void RouteMouseEvent(const blink::WebMouseEvent& event);
  virtual void RouteMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  virtual gfx::Rect GetBounds() const = 0;
  virtual void CopyFromSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      base::TimeDelta timeout,
      base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
          callback) = 0;
  virtual base::WeakPtr<UnboundedSurfaceWindow> GetWeakPtr() = 0;
  virtual void EnsureSurfaceSynchronizedForWebTest() = 0;

  virtual void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) = 0;

 protected:
  // Helper to schedule DestroyInternal() on the current thread.
  void ScheduleDeferredDestroy();

  // Tells the delegate, at most once, that the window is gone. Called on
  // destruction, and by subclasses when the platform window is destroyed
  // under them.
  void NotifyWindowDestroyed();

  // Platform-specific teardown step that destroys platform surfaces and
  // triggers deletion of this object via the parent view.
  virtual void TeardownAndDestroy() = 0;

  base::WeakPtr<RenderWidgetHostViewBase> parent_view_;
  base::WeakPtr<RenderWidgetHostViewBase> subframe_view_;

 private:
  // Destroys the surface, ending the window's lifetime.
  void DestroyInternal();

  template <typename EventType>
  RenderWidgetHostViewBase* PrepareEventForTargetView(EventType& web_event);

  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
