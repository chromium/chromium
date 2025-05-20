// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_state.h"

#include "base/metrics/histogram_macros.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

namespace {

// Prevent check on multiple workers per extension for testing purposes.
bool g_allow_multiple_workers_per_extension = false;

}  // namespace

ServiceWorkerState::ServiceWorkerState() = default;
ServiceWorkerState::~ServiceWorkerState() = default;

void ServiceWorkerState::SetBrowserState(BrowserState browser_state) {
  browser_state_ = browser_state;
}

void ServiceWorkerState::SetRendererState(RendererState renderer_state) {
  renderer_state_ = renderer_state;
}

void ServiceWorkerState::Reset() {
  worker_id_.reset();
  browser_state_ = BrowserState::kNotStarted;
  renderer_state_ = RendererState::kNotActive;
}

bool ServiceWorkerState::IsReady() const {
  return browser_state_ == BrowserState::kStarted &&
         renderer_state_ == RendererState::kActive && worker_id_.has_value();
}

void ServiceWorkerState::SetWorkerId(const WorkerId& worker_id,
                                     const ProcessManager* process_manager) {
  if (worker_id_ && *worker_id_ != worker_id) {
    // Sanity check that the old worker is gone.
    // TODO(crbug.com/40936639): remove
    // `g_allow_multiple_workers_per_extension` once bug is fixed so that this
    // DCHECK() will be default behavior everywhere. Also upgrade to a CHECK
    // once the bug is completely fixed.
    DCHECK(!process_manager->HasServiceWorker(*worker_id_) ||
           g_allow_multiple_workers_per_extension);

    // Clear stale renderer state if there's any.
    renderer_state_ = RendererState::kNotActive;
  }

  worker_id_ = worker_id;
}

void ServiceWorkerState::DidStartServiceWorkerContext(
    const WorkerId& worker_id,
    const ProcessManager* process_manager) {
  DCHECK_NE(RendererState::kActive, renderer_state())
      << "Worker already started";
  SetWorkerId(worker_id, process_manager);
  SetRendererState(RendererState::kActive);
}

void ServiceWorkerState::DidStartWorkerForScope(
    const WorkerId& worker_id,
    base::Time start_time,
    const ProcessManager* process_manager) {
  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.StartWorkerStatus",
                        true);
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.StartWorkerTime",
                      base::Time::Now() - start_time);

  DCHECK_NE(BrowserState::kStarted, browser_state())
      << "Worker was already loaded";

  SetWorkerId(worker_id, process_manager);
  SetBrowserState(BrowserState::kStarted);
}

// static
base::AutoReset<bool>
ServiceWorkerState::AllowMultipleWorkersPerExtensionForTesting() {
  return base::AutoReset<bool>(&g_allow_multiple_workers_per_extension, true);
}

}  // namespace extensions
