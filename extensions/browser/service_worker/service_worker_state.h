// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "extensions/browser/service_worker/worker_id.h"

namespace extensions {
class ProcessManager;

// The current worker related state of an activated extension.
class ServiceWorkerState {
 public:
  // Browser process worker state of an activated extension.
  enum class BrowserState {
    // Initial state, not started.
    kNotStarted,
    // Worker has completed starting at least once (i.e. has seen
    // DidStartWorkerForScope).
    kStarted,
    // Worker has completed starting at least once and has run all pending
    // tasks (i.e. has seen DidStartWorkerForScope and
    // DidStartServiceWorkerContext).
    kReady,
  };

  // Render process worker state of an activated extension.
  enum class RendererState {
    // Worker thread has not started or has been stopped/terminated.
    kNotActive,
    // Worker thread has started and it's running.
    kActive,
  };

  ServiceWorkerState();
  ~ServiceWorkerState();

  ServiceWorkerState(const ServiceWorkerState&) = delete;
  ServiceWorkerState& operator=(const ServiceWorkerState&) = delete;

  void SetBrowserState(BrowserState browser_state);
  void SetRendererState(RendererState renderer_state);
  void Reset();

  bool IsReady() const;

  // Called when a service worker renderer process is running, has executed its
  // global JavaScript scope, and all its global event listeners have been
  // registered with the //extensions layer. It is considered the
  // "renderer-side" signal that the worker is ready.
  void DidStartServiceWorkerContext(const WorkerId& worker_id,
                                    const ProcessManager* process_manager);

  // Called when the worker was requested to start and it verified that a worker
  // registration exists at the //content layer. It is considered the
  // "browser-side" signal that the worker is ready.
  void DidStartWorkerForScope(const WorkerId& worker_id,
                              base::Time start_time,
                              const ProcessManager* process_manager);

  BrowserState browser_state() const { return browser_state_; }
  RendererState renderer_state() const { return renderer_state_; }
  const std::optional<WorkerId>& worker_id() const { return worker_id_; }

  static base::AutoReset<bool> AllowMultipleWorkersPerExtensionForTesting();

 private:
  void SetWorkerId(const WorkerId& worker_id,
                   const ProcessManager* process_manager);

  BrowserState browser_state_ = BrowserState::kNotStarted;
  RendererState renderer_state_ = RendererState::kNotActive;

  // Contains the worker's WorkerId associated with this ServiceWorkerState,
  // once we have discovered info about the worker.
  std::optional<WorkerId> worker_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_
