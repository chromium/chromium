// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

class BackgroundFetchContext;
class BackgroundFetchRegistrationId;
class DevToolsBackgroundServicesContextImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Responsible for dispatching the Background Fetch API events on a given
// Service Worker. Must only be used on the UI thread.
class CONTENT_EXPORT BackgroundFetchEventDispatcher {
 public:
  // This enumeration is used for recording histograms. Treat as append-only.
  enum DispatchResult {
    DISPATCH_RESULT_SUCCESS = 0,
    DISPATCH_RESULT_CANNOT_FIND_WORKER = 1,
    DISPATCH_RESULT_CANNOT_START_WORKER = 2,
    DISPATCH_RESULT_CANNOT_DISPATCH_EVENT = 3,

    DISPATCH_RESULT_COUNT
  };

  BackgroundFetchEventDispatcher(
      BackgroundFetchContext* background_fetch_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      DevToolsBackgroundServicesContextImpl& devtools_context);

  BackgroundFetchEventDispatcher(const BackgroundFetchEventDispatcher&) =
      delete;
  BackgroundFetchEventDispatcher& operator=(
      const BackgroundFetchEventDispatcher&) = delete;

  ~BackgroundFetchEventDispatcher();

  // Dispatches one of the update, fail, or success events depending on the
  // provided registration.
  void DispatchBackgroundFetchCompletionEvent(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationDataPtr registration_data,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchclick` event, which indicates that the user
  // interface displayed for an active background fetch was activated.
  void DispatchBackgroundFetchClickEvent(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationDataPtr registration_data,
      base::OnceClosure finished_closure);

  // Called during during BackgroundFetchScheduler shutdown, during
  // StoragePartitionImpl destruction.
  void Shutdown();

 private:
  using ServiceWorkerLoadedCallback =
      base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                              int request_id)>;

  // Dispatches the `backgroundfetchabort` event, which indicates that an active
  // background fetch was aborted by the user or another external event.
  void DispatchBackgroundFetchAbortEvent(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchfail` event, which indicates that a
  // background fetch has finished with one or more failed fetches.
  void DispatchBackgroundFetchFailEvent(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchsuccess` event, which indicates that a
  // background fetch has successfully completed.
  void DispatchBackgroundFetchSuccessEvent(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      base::OnceClosure finished_closure);

  // Phase at which the dispatching process finished. Used for UMA.
  enum class DispatchPhase { FINDING, STARTING, DISPATCHING };

  // Loads the Service Worker identified included in the |registration_id| and
  // ensures that there is an activated version. Will invoke |finished_closure|,
  // log UMA and abort on error, or invoke |loaded_callback| on success.
  void LoadServiceWorkerRegistrationForDispatch(
      const BackgroundFetchRegistrationId& registration_id,
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback);

  // Verifies that the |registration| has successfully been loaded, then starts
  // the active Service Worker on the registration to dispatch |event|. Will
  // invoke |finished_closure|, log UMA and abort on error, or invoke the
  // |loaded_callback| on success.
  static void StartActiveWorkerForDispatch(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Dispatches the actual event after the Service Worker has been started.
  static void DispatchEvent(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      blink::ServiceWorkerStatusCode start_worker_status);

  // Called when an event of type |event| has finished dispatching.
  static void DidDispatchEvent(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      DispatchPhase dispatch_phase,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Methods that actually invoke the event on an activated Service Worker.
  static void DoDispatchBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);

  // Informs the DevToolsBackgroundServicesContextImpl of the completion event.
  void LogBackgroundFetchCompletionForDevTools(
      const BackgroundFetchRegistrationId& registration_id,
      ServiceWorkerMetrics::EventType event_type,
      blink::mojom::BackgroundFetchFailureReason failure_reason);

  // |background_fetch_context_| indirectly owns |this|.
  raw_ptr<BackgroundFetchContext> background_fetch_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Owned by StoragePartitionImpl; cleared during `Shutdown()`.
  raw_ptr<DevToolsBackgroundServicesContextImpl> devtools_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_
