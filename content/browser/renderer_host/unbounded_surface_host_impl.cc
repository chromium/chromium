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

  bounds_in_dips_ = bounds_in_dips;
  bounds_in_screen_ = ConstrainBounds(bounds_in_dips_);
  root_view.CreateUnboundedSurface(bounds_in_screen_, frame_view_);
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

// Run when zoom happens or the element's bounds change in the renderer.
void UnboundedSurfaceHostImpl::UpdateBounds(const gfx::Rect& bounds_in_dips) {
  bounds_in_dips_ = bounds_in_dips;
  if (!window_ || !root_view_ || !frame_view_) {
    return;
  }
  // Applied even if the screen bounds are unchanged: the renderer also resends
  // unchanged bounds when e.g. the device scale factor changes, which needs a
  // new LocalSurfaceId.
  ApplyWindowBounds(ConstrainBounds(bounds_in_dips_));
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
  // Keep the renderer in sync: disconnecting `client_` triggers programmatic
  // dismissal on the renderer (`UnboundedDismissReason::kProgrammatic`), which
  // cannot be canceled by `beforetoggle` since the window is already gone.
  dismiss_pending_ = true;
  client_.reset();
  DestroySoon();
}

// Recalculates bounds for the hit target (hitbox) when receiving an update from
// Viz, such as from zoom.
void UnboundedSurfaceHostImpl::OnTargetViewMoved() {
  if (!window_ || !root_view_ || !frame_view_ || dismiss_pending_) {
    return;
  }
  gfx::Rect bounds_in_screen = ConstrainBounds(bounds_in_dips_);
  if (bounds_in_screen != bounds_in_screen_) {
    ApplyWindowBounds(bounds_in_screen);
  }
}

void UnboundedSurfaceHostImpl::ApplyWindowBounds(
    const gfx::Rect& bounds_in_screen) {
  CHECK(window_);
  const viz::LocalSurfaceId previous_id = window_->GetLocalSurfaceId();
  bounds_in_screen_ = bounds_in_screen;
  window_->SetBounds(bounds_in_screen_);
  if (window_->GetLocalSurfaceId() != previous_id) {
    NotifySurfaceAllocated();
  }
}

// TODO(crbug.com/567606993): Bounds are recomputed when the renderer sends new
// bounds, and when the target view moves within the root view (see
// OnTargetViewMoved()), but not when the root view itself moves on screen
// without dismissing the surface (e.g. OS-initiated window moves, or when
// dismissal is canceled in beforetoggle), so the surface can be left in a
// stale position.
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
