// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_host_impl.h"

#include <algorithm>
#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {

namespace {

// Fallback timer duration to ensure the unbounded surface is destroyed if the
// renderer fails to respond.
constexpr base::TimeDelta kDismissFallbackTimeout = base::Seconds(10);

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(UnboundedSurfaceHostImpl);

UnboundedSurfaceHostImpl::UnboundedSurfaceHostImpl(
    RenderFrameHost* render_frame_host,
    RenderWidgetHostViewBase& root_view,
    RenderWidgetHostViewBase* frame_view,
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    const gfx::Rect& bounds_in_dips)
    : DocumentUserData<UnboundedSurfaceHostImpl>(render_frame_host),
      root_view_(root_view.GetWeakPtr()),
      frame_view_(frame_view ? frame_view->GetWeakPtr() : nullptr) {
  receiver_.Bind(std::move(host));
  receiver_.set_disconnect_handler(base::BindOnce(
      &UnboundedSurfaceHostImpl::DestroySoon, base::Unretained(this)));
  client_.Bind(std::move(client));

  // Only one surface may be active per root view. The previous surface's
  // host is notified through OnWindowDestroyed(), and tells its renderer, so
  // that both agree the previous element is dismissed (crbug.com/567605448).
  if (root_view.HasActiveUnboundedSurface()) {
    root_view.DestroyUnboundedSurface(
        root_view.GetUnboundedSurfaceWindow()->GetWeakPtr());
  }
  CHECK(!root_view.HasActiveUnboundedSurface());

  root_view.CreateUnboundedSurface(ConstrainBounds(bounds_in_dips),
                                   frame_view_);
  UnboundedSurfaceWindow* window = root_view.GetUnboundedSurfaceWindow();
  if (!window) {
    DestroySoon();
    return;
  }
  window_ = window->GetWeakPtr();
  window->set_delegate(this);
  NotifySurfaceAllocated();
}

UnboundedSurfaceHostImpl::~UnboundedSurfaceHostImpl() {
  if (window_) {
    window_->set_delegate(nullptr);
  }
  // No-op if the surface is already gone.
  if (root_view_) {
    root_view_->DestroyUnboundedSurface(window_);
  }
}

void UnboundedSurfaceHostImpl::GetCompositorFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) {
  if (window_) {
    window_->GetCompositorFrameSink(std::move(sink), std::move(client));
  }
}

void UnboundedSurfaceHostImpl::UpdateBounds(const gfx::Rect& bounds_in_dips) {
  if (!window_ || !root_view_) {
    return;
  }
  window_->SetBounds(ConstrainBounds(bounds_in_dips));
  NotifySurfaceAllocated();
}

void UnboundedSurfaceHostImpl::DidPresentFrameAfterDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_fallback_timer_.Stop();
  DestroySoon();
}

void UnboundedSurfaceHostImpl::DidCancelDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_pending_ = false;
  // TODO(crbug.com/567604594): Browser-initiated dismissal (outside click,
  // Esc, focus loss) goes through a cancelable beforetoggle event, and
  // stopping the fallback timer here lets the page keep the window open
  // indefinitely.
  dismiss_fallback_timer_.Stop();
}

void UnboundedSurfaceHostImpl::OnDismissRequested() {
  if (dismiss_pending_) {
    return;
  }
  dismiss_pending_ = true;
  client_->OnDismissed();
  dismiss_fallback_timer_.Start(
      FROM_HERE, kDismissFallbackTimeout,
      base::BindOnce(&UnboundedSurfaceHostImpl::DestroySoon,
                     base::Unretained(this)));
}

void UnboundedSurfaceHostImpl::OnWindowDestroyed() {
  window_ = nullptr;
  dismiss_fallback_timer_.Stop();
  // Keep the renderer in sync: from its point of view, the surface is
  // dismissed.
  if (!dismiss_pending_) {
    dismiss_pending_ = true;
    client_->OnDismissed();
  }
  DestroySoon();
}

// TODO(crbug.com/567606993): Constraints are only applied when the renderer
// sends new bounds. That doesn't happen when the window moves or when the
// parent moves an OOPIF, so the surface can be left in a stale position.
gfx::Rect UnboundedSurfaceHostImpl::ConstrainBounds(
    const gfx::Rect& bounds_in_dips) {
  CHECK(root_view_);
  gfx::Rect unconstrained_bounds =
      root_view_->ConvertSubframeBoundsToScreenWithoutConstraints(
          bounds_in_dips, frame_view_.get());
  std::optional<gfx::Rect> constraining_bounds =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetUnboundedElementConstrainingBoundsInScreen(root_view_.get());
  if (!constraining_bounds) {
    return unconstrained_bounds;
  }
  gfx::Rect constrained_bounds = unconstrained_bounds;
  constrained_bounds.Intersect(*constraining_bounds);
  if (constrained_bounds.IsEmpty()) {
    int max_x =
        std::max(constraining_bounds->x(), constraining_bounds->right() - 1);
    int max_y =
        std::max(constraining_bounds->y(), constraining_bounds->bottom() - 1);
    int clamped_x =
        std::clamp(unconstrained_bounds.x(), constraining_bounds->x(), max_x);
    int clamped_y =
        std::clamp(unconstrained_bounds.y(), constraining_bounds->y(), max_y);
    constrained_bounds = gfx::Rect(clamped_x, clamped_y, 1, 1);
  }
  return constrained_bounds;
}

void UnboundedSurfaceHostImpl::NotifySurfaceAllocated() {
  if (window_) {
    client_->OnSurfaceAllocated(window_->GetFrameSinkId(),
                                window_->GetLocalSurfaceId());
  }
}

void UnboundedSurfaceHostImpl::DestroySoon() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<UnboundedSurfaceHostImpl> self) {
                       if (self) {
                         DeleteForCurrentDocument(&self->render_frame_host());
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

}  // namespace content
