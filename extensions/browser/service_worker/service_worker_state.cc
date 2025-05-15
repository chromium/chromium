// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_state.h"

#include "extensions/browser/process_manager.h"

namespace extensions {

ServiceWorkerState::ServiceWorkerState() = default;
ServiceWorkerState::~ServiceWorkerState() = default;

void ServiceWorkerState::SetWorkerId(const WorkerId& worker_id) {
  if (worker_id_ && *worker_id_ != worker_id) {
    // Clear stale renderer state if there's any.
    renderer_state_ = RendererState::kNotActive;
  }
  worker_id_ = worker_id;
}

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

}  // namespace extensions
