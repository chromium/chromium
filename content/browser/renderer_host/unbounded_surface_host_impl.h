// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_HOST_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/unbounded_element/unbounded_element.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class RenderFrameHost;
class RenderWidgetHostViewBase;

// Browser side of an unbounded element: implements UnboundedSurfaceHost for
// the document that requested the surface.
//
// This object owns everything tied to the requesting document: the Mojo
// endpoints, the bounds constraints, and the dismissal handshake. The native
// window (UnboundedSurfaceWindow) is a pure platform object owned by the root
// RenderWidgetHostView, and is destroyed together with this object.
//
// Being a DocumentUserData, it is deleted when the RenderFrameHost is deleted
// or commits a new document. It is not deleted when the document stops being
// active (e.g. enters the BackForwardCache or starts unloading), nor when the
// renderer process crashes, so RenderFrameHostImpl deletes it explicitly in
// those cases.
class CONTENT_EXPORT UnboundedSurfaceHostImpl
    : public DocumentUserData<UnboundedSurfaceHostImpl>,
      public blink::mojom::UnboundedSurfaceHost,
      public UnboundedSurfaceWindow::Delegate {
 public:
  ~UnboundedSurfaceHostImpl() override;

  // blink::mojom::UnboundedSurfaceHost:
  void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client)
      override;
  void UpdateBounds(const gfx::Rect& bounds_in_dips) override;
  void DidPresentFrameAfterDismissal() override;
  void DidCancelDismissal() override;

  // UnboundedSurfaceWindow::Delegate:
  void OnDismissRequested() override;
  void OnWindowDestroyed() override;

 private:
  // `root_view` hosts the native window. `frame_view` is the view of the
  // requesting frame's local root; `bounds_in_dips` are relative to it.
  UnboundedSurfaceHostImpl(
      RenderFrameHost* render_frame_host,
      RenderWidgetHostViewBase& root_view,
      RenderWidgetHostViewBase* frame_view,
      mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
      mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient>
          client,
      const gfx::Rect& bounds_in_dips);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Converts `bounds_in_dips` to screen coordinates, and constrains them to
  // what the requesting document is allowed to cover.
  gfx::Rect ConstrainBounds(const gfx::Rect& bounds_in_dips);

  void NotifySurfaceAllocated();

  // Deletes `this`, and therefore the surface, in a posted task, so that
  // callers in active call stacks (Mojo dispatch, timers) can return first.
  void DestroySoon();

  base::WeakPtr<RenderWidgetHostViewBase> root_view_;
  base::WeakPtr<RenderWidgetHostViewBase> frame_view_;
  base::WeakPtr<UnboundedSurfaceWindow> window_;

  mojo::AssociatedReceiver<blink::mojom::UnboundedSurfaceHost> receiver_{this};
  mojo::AssociatedRemote<blink::mojom::UnboundedSurfaceClient> client_;

  bool dismiss_pending_ = false;
  base::OneShotTimer dismiss_fallback_timer_;

  base::WeakPtrFactory<UnboundedSurfaceHostImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_HOST_IMPL_H_
