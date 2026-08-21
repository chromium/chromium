// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace content {

namespace {

// Fallback timer duration to ensure the unbounded surface is destroyed if the
// renderer fails to respond.
constexpr base::TimeDelta kDismissFallbackTimeout = base::Seconds(10);

}  // namespace

UnboundedSurfaceWindow::UnboundedSurfaceWindow() = default;
UnboundedSurfaceWindow::~UnboundedSurfaceWindow() = default;

void UnboundedSurfaceWindow::Dismiss() {
  if (!IsValid() || dismiss_pending_) {
    return;
  }
  dismiss_pending_ = true;

  if (client_remote_.is_bound()) {
    client_remote_->OnDismissed();
  }

  dismiss_fallback_timer_.Start(
      FROM_HERE, kDismissFallbackTimeout,
      base::BindOnce(&UnboundedSurfaceWindow::ScheduleDeferredDestroy,
                     base::Unretained(this)));
}

void UnboundedSurfaceWindow::DidPresentFrameAfterDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_fallback_timer_.Stop();
  ScheduleDeferredDestroy();
}

void UnboundedSurfaceWindow::DidCancelDismissal() {
  if (!dismiss_pending_) {
    return;
  }
  dismiss_pending_ = false;
  dismiss_fallback_timer_.Stop();
}

void UnboundedSurfaceWindow::ScheduleDeferredDestroy() {
  // Destruction is deferred to a posted task so that callers in active call
  // stacks (e.g. Mojo message dispatchers or timer callbacks) can return safely
  // before the window and its associated platform objects are destroyed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UnboundedSurfaceWindow::DestroyInternal, GetWeakPtr()));
}

void UnboundedSurfaceWindow::DestroyInternal() {
  if (!dismiss_pending_) {
    return;
  }
  client_remote_.reset();
  TeardownAndDestroy();
}

}  // namespace content
