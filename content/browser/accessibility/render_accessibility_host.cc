// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/render_accessibility_host.h"

#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {
namespace {

// True if inbound accessibility calls are to be ignored.
std::atomic_bool g_renderer_serialization_experiment_enabled{false};

base::RepeatingClosure& GetOnAccessibilityDataDiscardedCallback() {
  static base::NoDestructor<base::RepeatingClosure> test_callback;
  return *test_callback;
}

}  // namespace

RenderAccessibilityHost::RenderAccessibilityHost(
    base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl,
    ui::AXTreeID tree_id)
    : render_frame_host_impl_(std::move(render_frame_host_impl)),
      tree_id_(tree_id) {}

RenderAccessibilityHost::~RenderAccessibilityHost() = default;

// static
void RenderAccessibilityHost::SetRendererSerializationExperimentEnabled(
    bool enabled) {
  g_renderer_serialization_experiment_enabled.store(enabled,
                                                    std::memory_order_relaxed);
}

// static
void RenderAccessibilityHost::SetAccessibilityDataDiscardedCallbackForTesting(
    base::RepeatingClosure closure) {
  GetOnAccessibilityDataDiscardedCallback() = std::move(closure);
}

void RenderAccessibilityHost::HandleAXEvents(
    const ui::AXUpdatesAndEvents& updates_and_events,
    const ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
    uint32_t reset_token,
    HandleAXEventsCallback callback) {
  NOTREACHED() << "Non-const ref version of this method should be used as a "
                  "performance optimization.";
}

void RenderAccessibilityHost::HandleAXEvents(
    ui::AXUpdatesAndEvents& updates_and_events,
    ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
    uint32_t reset_token,
    HandleAXEventsCallback callback) {
  if (g_renderer_serialization_experiment_enabled.load(
          std::memory_order_relaxed)) {
    std::move(callback).Run();
    if (GetOnAccessibilityDataDiscardedCallback()) {
      GetOnAccessibilityDataDiscardedCallback().Run();
    }
    return;
  }
  // Post the HandleAXEvents task onto the UI thread, and then when that
  // finishes, post back the response callback onto this runner (to satisfy
  // the mojo contract).
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::HandleAXEvents,
                     render_frame_host_impl_, tree_id_,
                     std::move(updates_and_events),
                     std::move(location_and_scroll_updates), reset_token,
                     mojo::GetBadMessageCallback()),
      std::move(callback));
}

void RenderAccessibilityHost::HandleAXLocationChanges(
    const ui::AXLocationAndScrollUpdates& changes,
    uint32_t reset_token) {
  NOTREACHED() << "Non-const ref version of this method should be used as a "
                  "performance optimization.";
}

void RenderAccessibilityHost::HandleAXLocationChanges(
    ui::AXLocationAndScrollUpdates& changes,
    uint32_t reset_token) {
  if (g_renderer_serialization_experiment_enabled.load(
          std::memory_order_relaxed)) {
    return;
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::HandleAXLocationChanges,
                     render_frame_host_impl_, tree_id_, std::move(changes),
                     reset_token, mojo::GetBadMessageCallback()));
}

}  // namespace content
