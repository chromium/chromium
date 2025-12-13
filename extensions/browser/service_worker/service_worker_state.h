// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "content/public/browser/service_worker_context.h"
#include "extensions/browser/service_worker/sequenced_context_id.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace extensions {
class ProcessManager;

// The current worker related state of an activated extension.
class ServiceWorkerState
    : public content::ServiceWorkerContextObserverSynchronous {
 public:
  // Browser process worker state of an activated extension.
  enum class BrowserState {
    // Worker has not started or has been stopped/terminated.
    kNotActive,
    // Worker has started (i.e. has seen DidStartWorkerForScope).
    kActive,
    // Worker has completed starting (i.e. has seen DidStartWorkerForScope and
    // DidStartServiceWorkerContext).
    // TODO(crbug.com/447640764): Remove this once
    // `OptimizeServiceWorkerStateRequests` is the default behavior.
    kReady,
  };

  // Render process worker state of an activated extension.
  enum class RendererState {
    // Worker thread has not started or has been stopped/terminated.
    kNotActive,
    // Worker thread has started and it's running.
    kActive,
  };

  ServiceWorkerState(content::ServiceWorkerContext* service_worker_context,
                     const ProcessManager* process_manager);
  ~ServiceWorkerState() override;

  ServiceWorkerState(const ServiceWorkerState&) = delete;
  ServiceWorkerState& operator=(const ServiceWorkerState&) = delete;

  class Observer : public base::CheckedObserver {
   public:
    // Called when an extension service worker is ready (both browser and
    // renderer sides are active).
    virtual void OnWorkerStart(const SequencedContextId& context_id,
                               const WorkerId& worker_id) {}
    // Called when an extension service worker has failed to start.
    virtual void OnWorkerStartFail(const SequencedContextId& context_id,
                                   base::Time start_time,
                                   content::StatusCodeResponse status) {}
    // Called when an extension service worker is stopping or has stopped.
    virtual void OnWorkerStop(int64_t version_id, const GURL& scope) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetBrowserState(BrowserState browser_state);
  void SetRendererState(RendererState renderer_state);
  void Reset();

  // Returns true if a request to start the worker has been made but the worker
  // is not ready yet.
  bool IsStarting() const;

  // Returns true if the worker is running and is ready to execute tasks.
  bool IsReady() const;

  // Starts the extension service worker. This method should only be called
  // if the service worker hasn't started yet. If this method is called while
  // the service worker is in the process of starting, it's a no-op.
  void StartWorker(const SequencedContextId& context_id);

  // Called when a service worker renderer process is running, has executed its
  // global JavaScript scope, and all its global event listeners have been
  // registered with the //extensions layer. It is considered the
  // "renderer-side" signal that the worker is ready.
  // NOTE: this can be called before or after `DidStartWorkerForScope`.
  void RendererDidStartServiceWorkerContext(
      const SequencedContextId& context_id,
      const WorkerId& worker_id);

  // Called when the render worker thread is preparing to terminate. It is
  // considered the "renderer-side" signal that the worker is stopping.
  // NOTE: this can be called before or after `OnStoppingSync` and
  // `OnStoppedSync`, or not at all.
  void RendererDidStopServiceWorkerContext(const WorkerId& worker_id,
                                           const GURL& scope);

  // Called when the worker was requested to start and it verified that a worker
  // registration exists at the //content layer. It is considered the
  // "browser-side" signal that the worker is ready.
  // NOTE: this can be called before or after
  // `RendererDidStartServiceWorkerContext`.
  void DidStartWorkerForScope(const SequencedContextId& context_id,
                              base::Time start_time,
                              int64_t version_id,
                              int process_id,
                              int thread_id);
  // Called when the worker was requested to start, but failed.
  void DidStartWorkerFail(const SequencedContextId& context_id,
                          base::Time start_time,
                          content::StatusCodeResponse status);

  BrowserState browser_state() const { return browser_state_; }
  RendererState renderer_state() const { return renderer_state_; }
  const std::optional<WorkerId>& worker_id() const { return worker_id_; }

  static base::AutoReset<bool> AllowMultipleWorkersPerExtensionForTesting();
  void StopObservingContextForTest();

  // content::ServiceWorkerContextObserverSynchronous:

  // Called when an extension service worker is stopping.
  // It is considered the "browser-side" signal that the worker is stopping.
  // NOTE: this can be called before or after
  // `RendererDidStopServiceWorkerContext`.
  void OnStoppingSync(int64_t version_id, const GURL& scope) override;

  // Called when an extension service worker has stopped.
  // It is considered the "browser-side" signal that the worker has stopped.
  // NOTE: this can be called before or after
  // `RendererDidStopServiceWorkerContext`.
  void OnStoppedSync(int64_t version_id, const GURL& scope) override;

 private:
  void SetWorkerId(const WorkerId& worker_id);
  void NotifyObserversIfReady(const SequencedContextId& context_id);
  void HandleStop(int64_t version_id, const GURL& scope);

  BrowserState browser_state_ = BrowserState::kNotActive;
  RendererState renderer_state_ = RendererState::kNotActive;

  // Whether the service worker is in the process of starting.
  bool worker_starting_ = false;

  // Contains the worker's WorkerId associated with this ServiceWorkerState,
  // once we have discovered info about the worker.
  std::optional<WorkerId> worker_id_;

  // Holds a pointer to the service worker context associated with this worker.
  const raw_ptr<content::ServiceWorkerContext> service_worker_context_ =
      nullptr;

  // Holds a pointer to the ProcessManager associated with a profile /
  // BrowserContext. This ServiceWorkerState is owned by ServiceWorkerTaskQueue,
  // ensuring ProcessManager outlives this instance.
  const raw_ptr<const ProcessManager> process_manager_ = nullptr;

  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserverSynchronous>
      service_worker_context_observation_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ServiceWorkerState> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STATE_H_
