// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TASK_QUEUE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/service_worker/sequenced_context_id.h"
#include "extensions/browser/service_worker/service_worker_state.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
struct ServiceWorkerRunningInfo;
}

namespace extensions {
class Extension;

// A service worker implementation of `LazyContextTaskQueue`. For an overview of
// service workers on the web see https://web.dev/learn/pwa/service-workers.
// Extension workers do not follow the typical web worker lifecycle. At a high
// level:
//   * only one worker instance should run at any given time for an extension
//     (e.g. there should not be a active and waiting version)
//   * only one worker version (version of its code) should run for each browser
//     session
//   * events can be dispatched to the worker before it is activated
//
// This class, despite being a task queue, does much more than just queue tasks
// for the worker. It handles worker registration, starting/stopping, and task
// readiness monitoring. The highlights to understand this class are:
//
// Worker Registration:
//
// Worker registration must occur in order to start a worker for the extension.
// Otherwise requests to start a worker will fail. Service worker registration
// is persisted to disk in the //content layer to avoid unnecessary registration
// requests. This prevents a registration request for every restart of the
// browser. If there’s a registration record the registration is still verified
// with the //content layer).
//
// Worker Started/Stopped:
//
// Starting:
//
// A worker must be started before it can become ready to process the event
// tasks. When a task arrives, the task queue checks if the worker is ready. If
// the worker is ready (and `OptimizeServiceWorkerStartRequests` is enabled),
// the task is dispatched immediately. If the worker is not ready (or the
// optimization is disabled), the task is queued, and the queue requests the
// worker to start (if it hasn't already).
//
// `RendererDidStartServiceWorkerContext()` is called asynchronously from the
// extension renderer process (potentially before or after
// `DidStartWorkerForScope()`) and it records that the worker has started in the
// renderer (process).
//
// Stopping:
//
// The worker stopping sequence involves signals from both the renderer and the
// browser process.
//   * Renderer-side: `RendererDidStopServiceWorkerContext()` is called when the
//     worker is stopped to track renderer stopping. It is not always guaranteed
//     to be called (e.g. if the render process is destroyed).
//   * Browser-side: `OnStoppingSync()` and `OnStoppedSync()` are called by the
//     content layer. We expect this to always be called.
// `ServiceWorkerState` observes all these signals and immediately resets its
// internal state when any stop signal arrives for the currently tracked worker,
// marking the worker as not ready for new tasks.
//
// Task Processing Readiness:
//
// Three worker started signals are together used to determine when a worker is
// ready to process tasks. Due to this it makes the process more complicated
// than just checking if the worker is “running” (e.g by calling the //content
// layer for this).
//
// A worker is checked for readiness by its worker state. Readiness checks three
// signals: `BrowserState`, `RendererState`, and `WorkerId` that are each set by
// certain methods:
//   * `BrowserState`: `DidStartWorkerForScope()` signal sets the value to
//     ready. This signal means that the worker was *requested* to start and it
//     verified that a worker registration exists at the //content layer. It is
//     considered the “browser-side” signal that the worker is ready.
//   * `RendererState`: `RendererDidStartServiceWorkerContext()` signal sets the
//     value to ready. This is start requests are sent to the worker. This
//     signal means:
//       * that there is a worker renderer process thread running the service
//         worker code
//       * the worker has done one pass and executed it’s entire JS global scope
//       * as part of executing that scope: the worker has registered all its
//         (top-level/global) event listeners with the //extensions layer (all
//         event listener mojom calls have been received and processed). This
//         ordering is guaranteed because the mojom message that calls this
//         signal is after the event listener mojom messages on an associated
//         mojom pipe.
//   * `worker_id_.has_value()`: this signal confirms that
//     the class is populated with the running service worker’s information
//     (render process and thread id, and worker version id) . This confirms
//     that when the task is dispatched to the worker it is sent to the running
//     worker (and not a previously stopped one).
//
// Ordering of Registration and Start Worker Completion:
//
// Note that while worker registration in //content `DidRegisterServiceWorker()`
// will finish before requesting the worker to start, there is no guarantee on
// how the signals for their completion will be received.
// For example `DidRegisterServiceWorker()`, `DidStartWorkerForScope()` and
// `RendererDidStartServiceWorkerContext()` signals are not guaranteed to
// finish in any order.
//
// Activation Token:
//
// When an extension is activated (e.g., loaded or updated), a unique
// `activation_token` is generated. This token, combined with `extension_id` and
// `browser_context_id`, forms the `SequencedContextId`. This ID is used
// internally to track the state, pending tasks, and registration status
// specific to that activation sequence. If the extension is deactivated and
// reactivated, a new token is generated, allowing the system to discard stale
// state and callbacks from the previous activation sequence. This ensures that
// tasks from a previous instance of an extension don't get dispatched to a new
// running instance.
class ServiceWorkerTaskQueue
    : public KeyedService,
      public LazyContextTaskQueue,
      public content::ServiceWorkerContextObserverSynchronous,
      public ServiceWorkerState::Observer {
 public:
  explicit ServiceWorkerTaskQueue(content::BrowserContext* browser_context);

  ServiceWorkerTaskQueue(const ServiceWorkerTaskQueue&) = delete;
  ServiceWorkerTaskQueue& operator=(const ServiceWorkerTaskQueue&) = delete;

  ~ServiceWorkerTaskQueue() override;

  // Convenience method to return the ServiceWorkerTaskQueue for a given
  // `context`.
  static ServiceWorkerTaskQueue* Get(content::BrowserContext* context);

  // Returns true if the task should be queued before being dispatched.
  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension) const override;

  // Returns true if the service worker is running and ready to execute tasks.
  bool IsReadyToRunTasks(content::BrowserContext* context,
                         const Extension* extension) const override;

  // Adds a task. If the worker is ready (and optimizations are enabled), the
  // task may be dispatched immediately. Otherwise, the task is queued, and the
  // worker is started if necessary.
  void AddPendingTask(const LazyContextId& context_id,
                      PendingTask task) override;

  // Performs Service Worker related tasks upon `extension` activation,
  // e.g. registering `extension`'s worker, executing any pending tasks.
  void ActivateExtension(const Extension* extension);
  // Performs Service Worker related tasks upon `extension` deactivation,
  // e.g. unregistering `extension`'s worker.
  void DeactivateExtension(const Extension* extension);

  // Called once an extension Service Worker context was initialized but not
  // necessarily started executing its JavaScript.
  void RendererDidInitializeServiceWorkerContext(
      int render_process_id,
      const ExtensionId& extension_id,
      int64_t service_worker_version_id,
      int thread_id,
      const blink::ServiceWorkerToken& service_worker_token);
  // Called once an extension Service Worker started running.
  // This can be thought as "loadstop", i.e. the global JS script of the worker
  // has completed executing.
  void RendererDidStartServiceWorkerContext(
      int render_process_id,
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int thread_id);
  // Called once an extension Service Worker was destroyed.
  void RendererDidStopServiceWorkerContext(
      int render_process_id,
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int thread_id);
  // Called when the extension renderer process that was running an extension
  // Service Worker has exited.
  void RenderProcessForWorkerExited(const WorkerId& worker_id);

  // Returns the current activation token for an extension, if the extension
  // is currently activated. Returns std::nullopt if the extension isn't
  // activated.
  std::optional<base::UnguessableToken> GetCurrentActivationToken(
      const ExtensionId& extension_id) const;

  // Activates incognito split mode extensions that are activated in `other`
  // task queue.
  void ActivateIncognitoSplitModeExtensions(ServiceWorkerTaskQueue* other);

  // Retrieves the version of the extension that has currently registered
  // and stored an extension service worker. If there is no such version (if
  // there was never a service worker or if the worker was unregistered),
  // returns an invalid version.
  base::Version RetrieveRegisteredServiceWorkerVersion(
      const ExtensionId& extension_id);

  // ServiceWorkerState::Observer:
  void OnWorkerStart(const SequencedContextId& context_id,
                     const WorkerId& worker_id) override;
  void OnWorkerStartFail(const SequencedContextId& context_id,
                         base::Time start_time,
                         content::StatusCodeResponse status) override;
  void OnWorkerStop(int64_t version_id, const GURL& scope) override;

  // content::ServiceWorkerContextObserverSynchronous:
  void OnRegistrationStoredSync(int64_t registration_id,
                                const GURL& scope) override;
  void OnReportConsoleMessageSync(
      int render_process_id,
      int64_t version_id,
      const GURL& scope,
      const content::ConsoleMessage& message) override;
  void OnDestructSync(content::ServiceWorkerContext* context) override;

  // Worker unregistrations can fail in expected and unexpected ways, this
  // determines if the unregistration can be accepted as successful from the
  // extension's perspective. When there was a record of worker registration
  // prior to unregistering, `worker_previously_registered` should be set to
  // true. Used in metrics.
  bool IsWorkerUnregistrationSuccess(blink::ServiceWorkerStatusCode status_code,
                                     bool worker_previously_registered);
  // Whether this class is aware of a worker being registered. Note: This does
  // not verify that the registration exists in the service worker layer, so it
  // may not be 100% accurate (if there are bugs in registration tracking logic
  // in this class). Used in metrics.
  bool IsWorkerRegistered(const ExtensionId extension_id);

  class TestObserver {
   public:
    TestObserver();

    TestObserver(const TestObserver&) = delete;
    TestObserver& operator=(const TestObserver&) = delete;

    virtual ~TestObserver();

    // Called when an extension with id `extension_id` is going to be activated.
    // `will_register_service_worker` is true if a Service Worker will be
    // registered.
    virtual void OnActivateExtension(const ExtensionId& extension_id,
                                     bool will_register_service_worker) {}

    // Called immediately after we send a request to start the worker (whether
    // it ultimately succeeds or fails).
    virtual void RequestedWorkerStart(const ExtensionId& extension_id) {}

    virtual void OnWorkerRegistrationFailed(
        const ExtensionId& extension_id,
        blink::ServiceWorkerStatusCode status_code) {}

    virtual void DidStartWorkerFail(
        const ExtensionId& extension_id,
        size_t num_pending_tasks,
        blink::ServiceWorkerStatusCode status_code) {}

    // Called when SW was re-registered to fix missing registration, and that
    // step finished to mitigate the problem.
    virtual void RegistrationMismatchMitigated(const ExtensionId& extension_id,
                                               bool mitigation_succeeded) {}

    // Called when a service worker is registered for the extension with the
    // associated `extension_id`.
    virtual void RendererDidInitializeServiceWorkerContext(
        const ExtensionId& extension_id) {}

    // Called when a service worker is fully started
    // (`RendererDidStartWorkerForScope()` and
    // `RendererDidStartServiceWorkerContext()` were called) for the extension
    // with the associated `extension_id`.
    virtual void DidStartWorker(const ExtensionId& extension_id) {}

    // Called when a service worker registered for the extension with the
    // `extension_id` has notified the task queue that the render worker thread
    // is preparing to terminate.
    virtual void RendererDidStopServiceWorkerContext(
        const ExtensionId& extension_id) {}

    // Called when UntrackServiceWorkerState() is invoked for a worker
    // associated with `scope` (because it's stopping or has stopped).
    // This notification occurs even if the worker is a sub-scope worker and
    // does not result in altering the ServiceWorkerTaskQueue's tracking state
    // for the primary extension service worker.
    virtual void UntrackServiceWorkerState(const GURL& scope) {}

    // Called when a service worker registered for the extension with the
    // `extension_id` has been unregistered in the //content layer.
    virtual void WorkerUnregistered(const ExtensionId& extension_id) {}

    // Called when a service worker registered for the extension with the
    // `extension_id` has been registered in the //content layer. It is always
    // called, even if the registration request fails.
    virtual void OnWorkerRegistered(const ExtensionId& extension_id) {}
  };

  static void SetObserverForTest(TestObserver* observer);

  size_t GetNumPendingTasksForTest(const LazyContextId& lazy_context_id);

  ServiceWorkerState* GetWorkerStateForTesting(
      const SequencedContextId& context_id) {
    return GetWorkerState(context_id);
  }

 private:
  enum class RegistrationReason {
    REGISTER_ON_EXTENSION_LOAD,
    RE_REGISTER_ON_STATE_MISMATCH,
    RE_REGISTER_ON_TIMEOUT,
  };

  // Manages registration/start retry attempts with exponential backoff.
  struct RetryState;

  // KeyedService:
  void Shutdown() override;

  // Untracks the service worker from any state that believe the worker in ready
  // to receive extension events.
  void UntrackServiceWorkerState(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& worker_info);

  void RegisterServiceWorker(RegistrationReason reason,
                             const SequencedContextId& context_id,
                             const Extension& extension);

  // Dispatches the given tasks to the service worker associated to the given
  // context. Requires the worker to be ready.
  void DispatchTasksImmediately(const SequencedContextId& context_id,
                                base::span<PendingTask> tasks);

  // Returns true if a service worker start failure is transient and should be
  // retried, false otherwise.
  bool IsStartFailureRetryable(
      blink::ServiceWorkerStatusCode status_code) const;

  // Callbacks called when the worker is registered or unregistered,
  // respectively. `worker_previously_successfully_registered` true indicates
  // that when the unregistration request was made the task queue had a record
  // of an existing worker registration.
  void DidRegisterServiceWorker(const SequencedContextId& context_id,
                                RegistrationReason reason,
                                base::Time start_time,
                                blink::ServiceWorkerStatusCode status);
  void DidUnregisterServiceWorker(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      bool worker_previously_registered,
      blink::ServiceWorkerStatusCode status);

  // Worker registrations can fail in expected and unexpected ways, this
  // determines if the registration can be accepted as successful from the
  // extension's perspective.
  bool IsWorkerRegistrationSuccess(blink::ServiceWorkerStatusCode status);

  bool IsStartWorkerFailureUnexpected(
      blink::ServiceWorkerStatusCode status_code);

  // Records that the extension with `extension_id` and `version` successfully
  // registered a Service Worker.
  void SetRegisteredServiceWorkerInfo(const ExtensionId& extension_id,
                                      const base::Version& version);

  // Clears any record of registered Service Worker for the given extension with
  // `extension_id`.
  void RemoveRegisteredServiceWorkerInfo(const ExtensionId& extension_id);

  // Returns true if `activation_token` is the current activation for
  // `extension_id`.
  bool IsCurrentActivation(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token) const;

  // Gets the worker state and context ID for a given activation.
  // If the activation is not current, returns a null worker state.
  std::tuple<ServiceWorkerState*, SequencedContextId>
  GetWorkerStateForActivation(const ExtensionId& extension_id,
                              const base::UnguessableToken& activation_token);

  const ServiceWorkerState* GetWorkerState(
      const SequencedContextId& context_id) const;
  ServiceWorkerState* GetWorkerState(const SequencedContextId& context_id);

  content::ServiceWorkerContext* GetServiceWorkerContext(
      const ExtensionId& extension_id);

  // Starts and stops observing `service_worker_context`.
  //
  // The methods ensure that many:1 relationship of SWContext:SWContextObserver
  // is preserved correctly.
  void StartObserving(content::ServiceWorkerContext* service_worker_context);
  void StopObserving(content::ServiceWorkerContext* service_worker_context);

  // Asynchronously verifies whether an expected SW registration (denoted by
  // `scope`) is there.
  void VerifyRegistration(content::ServiceWorkerContext* service_worker_context,
                          const SequencedContextId& context_id,
                          const GURL& scope);
  void DidVerifyRegistration(const SequencedContextId& context_id,
                             content::ServiceWorkerCapability capability);

  // Emit histograms when we know we're going to start the worker.
  void EmitWorkerWillBeStartedHistograms(const ExtensionId& extension_id);

  // Returns the pending tasks for the activated extension. This returns
  // `nullptr` if the vector has not been created yet for `context_id`. Should
  // return non-null after activating extension and before deactivating
  // extension.
  std::vector<PendingTask>* pending_tasks(const SequencedContextId& context_id);

  // Returns the pending tasks for the activated extension. This creates an
  // empty `std::vector<PendingTask>` for `context_id` if there is not one yet.
  // TODO(crbug.com/40276609): Can we ensure `context_id` key has been set
  // before this is called so we don't need to add it?
  std::vector<ServiceWorkerTaskQueue::PendingTask>& GetOrAddPendingTasks(
      const SequencedContextId& context_id);

  // Adds a pending task for the activated extension.
  void AddPendingTaskForContext(PendingTask&& pending_task,
                                const SequencedContextId& context_id);

  // Runs any pending tasks associated with `context_id` with a null context
  // (indicating failure) and clears them from the queue.
  void RunAndClearPendingTasksWithNullContext(
      const SequencedContextId& context_id);

  // Whether there are any pending tasks to run for the activated extension.
  bool HasPendingTasks(const SequencedContextId& context_id);

  // Starts service worker, unless it's already in the process of starting.
  void MaybeStartWorker(ServiceWorkerState* worker_state,
                        const SequencedContextId& context_id);

  // Attempts to register the worker again after a previous failure.
  void RetryRegisterServiceWorker(const SequencedContextId& context_id,
                                  RegistrationReason reason);

  // Attempts to start the worker again after a previous failure.
  void RetryStartWorker(const SequencedContextId& context_id);

  using RetryMap =
      std::map<base::UnguessableToken, std::unique_ptr<RetryState>>;

  // Schedules a retry attempt using the provided map. Returns true if a retry
  // was scheduled, false if retries are exhausted.
  bool ScheduleRetry(const base::UnguessableToken& token,
                     RetryMap& retry_map,
                     base::OnceClosure retry_callback);

  // Clears retry state from the given map and logs metrics.
  void ClearRetryState(const base::UnguessableToken& token,
                       RetryMap& retry_map,
                       const char* histogram_name,
                       bool success);

  // Whether the task queue (as a keyed service) has been informed that the
  // browser context is shutting down. Used for metrics purposes.
  bool browser_context_shutting_down_ = false;

  std::map<content::ServiceWorkerContext*, int> observing_worker_contexts_;

  // The state of worker of each activated extension.
  base::flat_map<SequencedContextId, std::unique_ptr<ServiceWorkerState>>
      worker_state_map_;

  // NOTE: this needs to come after `worker_state_map_` to ensure the observers
  // are removed before the `ServiceWorkerState`s are cleaned up.
  base::ScopedMultiSourceObservation<ServiceWorkerState,
                                     ServiceWorkerState::Observer>
      worker_state_observations_{this};

  // TODO(crbug.com/40276609): Do we need to track this by `SequencedContextId`
  // or could we use `ExtensionId` instead?
  // `PendingTasks` for the activated extension that will be run as soon as the
  // worker is started and ready.
  std::map<SequencedContextId, std::vector<PendingTask>> pending_tasks_map_;

  const raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // A map of Service Worker registrations if this instance is for an
  // off-the-record BrowserContext. These are stored in the ExtensionPrefs
  // for a regular profile.
  // TODO(crbug.com/40617251): Make this better by passing in something that
  // will manage storing and retrieving this data.
  base::flat_map<ExtensionId, base::Version> off_the_record_registrations_;

  // Current activation tokens for each activated extensions.
  std::map<ExtensionId, base::UnguessableToken> activation_tokens_;

  // Manages worker start retries for an activation token.
  RetryMap worker_start_retries_;

  // Manages worker registration retries for an activation token.
  RetryMap worker_registration_retries_;

  // A set of service worker registrations that are pending storage.
  // These are registrations that succeeded in the first step (triggering
  // `DidRegisterServiceWorker`), but have not yet been stored.
  // They are cleared out (and the registration state is stored) in response to
  // `OnRegistrationStoredSync`. The key is the extension's ID and the value is
  // the activation token expected for that registration.
  std::map<ExtensionId, base::UnguessableToken> pending_storage_registrations_;

  // TODO(crbug.com/40276609): Do we need to track this by `SequencedContextId`
  // or could we used `ExtensionId` instead?
  // The activated extensions that have workers that are registered with the
  // //content layer.
  std::set<SequencedContextId> worker_registered_;

  base::WeakPtrFactory<ServiceWorkerTaskQueue> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TASK_QUEUE_H_
