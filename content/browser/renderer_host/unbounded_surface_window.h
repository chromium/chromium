// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/blink/public/mojom/unbounded_element/unbounded_element.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace blink {
class WebMouseEvent;
}

namespace content {

class CONTENT_EXPORT UnboundedSurfaceWindow
    : public blink::mojom::UnboundedSurfaceHost {
 public:
  UnboundedSurfaceWindow();
  ~UnboundedSurfaceWindow() override;

  virtual bool IsValid() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Shared lifecycle and dismissal logic.
  // To initiate the dismissal of this window, call Dismiss().
  void Dismiss();

  virtual void SetBounds(const gfx::Rect& bounds_in_screen) = 0;
  virtual viz::FrameSinkId GetFrameSinkId() const = 0;
  virtual viz::LocalSurfaceId GetLocalSurfaceId() const = 0;

  virtual void RouteMouseEvent(const blink::WebMouseEvent& event) = 0;
  virtual gfx::Rect GetBounds() const = 0;
  virtual void CopyFromSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      base::TimeDelta timeout,
      base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
          callback) = 0;
  virtual base::WeakPtr<UnboundedSurfaceWindow> GetWeakPtr() = 0;
  virtual void EnsureSurfaceSynchronizedForWebTest() = 0;

  // blink::mojom::UnboundedSurfaceHost overrides:
  void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client)
      override = 0;
  void UpdateBounds(const gfx::Rect& bounds) override = 0;
  // Called when the client has completed its dismissal animation and submitted
  // a frame. At this point, it is safe to destroy the window.
  void DidPresentFrameAfterDismissal() override;
  // Called if the client decides to cancel the dismissal.
  void DidCancelDismissal() override;

 protected:
  // Helper to schedule DestroyInternal() on the current thread.
  void ScheduleDeferredDestroy();

  // Platform-specific teardown step that destroys platform surfaces and
  // triggers deletion of this object via the parent view.
  virtual void TeardownAndDestroy() = 0;

  bool dismiss_pending_ = false;
  mojo::AssociatedRemote<blink::mojom::UnboundedSurfaceClient> client_remote_;

 private:
  // Destroys the surface, ending the window's lifetime.
  void DestroyInternal();

  base::OneShotTimer dismiss_fallback_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
