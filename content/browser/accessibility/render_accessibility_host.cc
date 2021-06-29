// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/render_accessibility_host.h"

#include <memory>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

RenderAccessibilityHost::RenderAccessibilityHost(
    base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl,
    mojo::PendingReceiver<mojom::RenderAccessibilityHost> receiver,
    ui::AXTreeID tree_id)
    : render_frame_host_impl_(std::move(render_frame_host_impl)),
      receiver_{this, std::move(receiver)},
      tree_id_(tree_id) {}

RenderAccessibilityHost::~RenderAccessibilityHost() = default;

void RenderAccessibilityHost::HandleAXEvents(
    mojom::AXUpdatesAndEventsPtr updates_and_events,
    int32_t reset_token,
    HandleAXEventsCallback callback) {
  // Post the HandleAXEvents task onto the UI thread, and then when that
  // mojo contract).
  base::PostTaskAndReply(
      // finishes, post back the response callback onto this runner (to satisfy
      // the
      FROM_HERE, BrowserThread::ID::UI,
      base::BindOnce(&RenderFrameHostImpl::HandleAXEvents,
                     render_frame_host_impl_, tree_id_,
                     std::move(updates_and_events), reset_token),
      std::move(callback));
}

void RenderAccessibilityHost::HandleAXLocationChanges(
    std::vector<content::mojom::LocationChangesPtr> changes) {
  base::PostTask(
      FROM_HERE, BrowserThread::ID::UI,
      base::BindOnce(&RenderFrameHostImpl::HandleAXLocationChanges,
                     render_frame_host_impl_, tree_id_, std::move(changes)));
}

}  // namespace content
