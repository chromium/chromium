// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/browser/service_worker_external_request_timeout_type.h"
#include "content/public/browser/service_worker_running_info.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom-forward.h"

namespace base {
class Uuid;
}

namespace blink {
class AssociatedInterfaceProvider;
class StorageKey;
}  // namespace blink

namespace service_manager {
class InterfaceProvider;
}

namespace url {
class Origin;
}  // namespace url

class GURL;

namespace content {

class ServiceWorkerContextObserver;

struct ServiceWorkerRunningInfo;
struct StorageUsageInfo;

enum class ServiceWorkerCapability {
  NO_SERVICE_WORKER,
  SERVICE_WORKER_NO_FETCH_HANDLER,
  SERVICE_WORKER_WITH_FETCH_HANDLER,
};

// Used for UMA. Append only.
enum class StartServiceWorkerForNavigationHintResult {
  // The service worker started successfully.
  STARTED = 0,
  // The service worker was already running.
  ALREADY_RUNNING = 1,
  // There was no service worker registration for the given URL.
  NO_SERVICE_WORKER_REGISTRATION = 2,
  // There was no active service worker for the given URL.
  NO_ACTIVE_SERVICE_WORKER_VERSION = 3,
  // The service worker for the given URL had no fetch event handler.
  NO_FETCH_HANDLER = 4,
  // Something failed.
  FAILED = 5,
  // Add new result to record here.
  kMaxValue = FAILED,
};

// A callback invoked with the result of executing script in a service worker
// context.
using ServiceWorkerScriptExecutionCallback =
    base::OnceCallback<void(base::Value value,
                            const std::optional<std::string>& error)>;

// An observer very similar to `ServiceWorkerContextCoreObserver`, but meant to
// only be used by extension service workers. Its methods are called
// synchronously with changes in //content.
class ServiceWorkerContextObserverSynchronous : public base::CheckedObserver {
 public:
  // Called when the service worker with id `version_id` has stopped running.
  virtual void OnStopped(int64_t version_id,
                         const ServiceWorkerRunningInfo& worker_info) {}

  // TODO(crbug.com/334940006): Add the rest of the extensions methods
  // (OnRegistrationStored(), OnReportConsoleMessage(), OnDestruct()) and adapt
  // `ServiceWorkerTaskQueue` to use this observer exclusively.
};

// Represents the per-StoragePartition service worker data.
//
// See service_worker_context_wrapper.cc for the implementation
// of ServiceWorkerContext and ServiceWorkerContextWrapper (the
// primary implementation of this abstract class).
//
// All methods must be called on the UI thread.
class CONTENT_EXPORT ServiceWorkerContext {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  using GetInstalledRegistrationOriginsCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;

  using GetUsageInfoCallback =
      base::OnceCallback<void(const std::vector<StorageUsageInfo>& usage_info)>;

  using CheckHasServiceWorkerCallback =
      base::OnceCallback<void(ServiceWorkerCapability capability)>;

  using StatusCodeCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status_code)>;

  using StartServiceWorkerForNavigationHintCallback = base::OnceCallback<void(
      StartServiceWorkerForNavigationHintResult result)>;

  using WarmUpServiceWorkerCallback = base::OnceClosure;

  using StartWorkerCallback = base::OnceCallback<
      void(int64_t version_id, int process_id, int thread_id)>;

  // Returns true if |url| is within the service worker |scope|.
  static bool ScopeMatches(const GURL& scope, const GURL& url);

  // Runs a |task| on task |runner| making sure that
  // |service_worker_context| is alive while the task is being run.
  static void RunTask(scoped_refptr<base::SequencedTaskRunner> runner,
                      const base::Location& from_here,
                      ServiceWorkerContext* service_worker_context,
                      base::OnceClosure task);

  // Returns the delay from navigation to starting an update of a service
  // worker's script.
  static base::TimeDelta GetUpdateDelay();

  // Add/remove an observer that is asynchronously notified.
  virtual void AddObserver(ServiceWorkerContextObserver* observer) = 0;
  virtual void RemoveObserver(ServiceWorkerContextObserver* observer) = 0;
  // Add/remove an observer that is synchronously notified.
  virtual void AddSyncObserver(
      ServiceWorkerContextObserverSynchronous* observer) {}
  virtual void RemoveSyncObserver(
      ServiceWorkerContextObserverSynchronous* observer) {}

  // Equivalent to calling navigator.serviceWorker.register(script_url,
  // options) for a given `key`. `callback` is passed true when the JS promise
  // is fulfilled or false when the JS promise is rejected.
  //
  // The registration can fail if:
  //  * `script_url` is on a different origin from `scope`
  //  * Fetching `script_url` fails.
  //  * `script_url` fails to parse or its top-level execution fails.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  virtual void RegisterServiceWorker(
      const GURL& script_url,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      StatusCodeCallback callback) = 0;

  // Equivalent to calling ServiceWorkerRegistration#unregister on the
  // registration for `scope`.
  //
  // Unregistration can fail if:
  //  * No registration exists for `scope`.
  //  * Something unexpected goes wrong, like a renderer crash.
  //
  // `callback` provides the status code result of the unregistration.
  // `blink::ServiceWorkerStatusCode::kOk` means the request to unregister was
  // sent. It does not mean the worker has been fully unregistered though.
  virtual void UnregisterServiceWorker(const GURL& scope,
                                       const blink::StorageKey& key,
                                       StatusCodeCallback callback) = 0;
  // As above, but clears the service worker registration immediately rather
  // than waiting if the service worker is active and has controllees.
  virtual void UnregisterServiceWorkerImmediately(
      const GURL& scope,
      const blink::StorageKey& key,
      StatusCodeCallback callback) = 0;

  // Mechanism for embedder to increment/decrement ref count of a service
  // worker.
  //
  // Embedders can call StartingExternalRequest() while it is performing some
  // work with the worker. The worker is considered to be working until embedder
  // calls FinishedExternalRequest(). This ensures that content/ does not
  // shut the worker down while embedder is expecting the worker to be kept
  // alive.
  virtual ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      ServiceWorkerExternalRequestTimeoutType timeout_type,
      const base::Uuid& request_uuid) = 0;
  virtual ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const base::Uuid& request_uuid) = 0;

  // Returns the pending external request count for the worker with the
  // specified `key`.
  virtual size_t CountExternalRequestsForTest(const blink::StorageKey& key) = 0;

  // Executes the given `script` in the context of the worker specified by the
  // given `service_worker_version_id`. If non-empty, `callback` is invoked
  // with the result of the script. See also service_worker.mojom.
  virtual bool ExecuteScriptForTest(
      const std::string& script,
      int64_t service_worker_version_id,
      ServiceWorkerScriptExecutionCallback callback) = 0;

  // Whether `key` has any registrations. Uninstalling and uninstalled
  // registrations do not cause this to return true, that is, only registrations
  // with status ServiceWorkerRegistration::Status::kIntact are considered, such
  // as even if the corresponding live registrations may still exist. Also,
  // returns true if it doesn't know (registrations are not yet initialized).
  virtual bool MaybeHasRegistrationForStorageKey(
      const blink::StorageKey& key) = 0;

  // Used in response to browsing data and quota manager requests to get
  // the per-StorageKey size and last time used data.
  virtual void GetAllStorageKeysInfo(GetUsageInfoCallback callback) = 0;

  // Deletes all registrations for `key` and clears all service workers
  // belonging to the registrations. All clients controlled by those service
  // workers will lose their controllers immediately after this operation.
  virtual void DeleteForStorageKey(const blink::StorageKey& key,
                                   ResultCallback callback) = 0;

  // Returns ServiceWorkerCapability describing existence and properties of a
  // Service Worker registration matching `url` and `key`. In case the service
  // worker is being installed as of calling this method, it will wait for the
  // installation to finish before coming back with the result.
  virtual void CheckHasServiceWorker(
      const GURL& url,
      const blink::StorageKey& key,
      CheckHasServiceWorkerCallback callback) = 0;

  // Stops all running service workers and unregisters all service worker
  // registrations. This method is used in web tests to make sure that the
  // existing service worker will not affect the succeeding tests.
  virtual void ClearAllServiceWorkersForTest(base::OnceClosure callback) = 0;

  // Starts the active worker of the registration for the given `scope` and
  // `key`. If there is no active worker, starts the installing worker.
  // `info_callback` is passed information about the started worker if
  // successful, otherwise `failure_callback` is passed information about the
  // error.
  //
  // There is no guarantee about whether the callback is called synchronously or
  // asynchronously.
  virtual void StartWorkerForScope(const GURL& scope,
                                   const blink::StorageKey& key,
                                   StartWorkerCallback info_callback,
                                   StatusCodeCallback failure_callback) = 0;

  // Starts the active worker of the registration for the given `scope` and
  // `key` and dispatches the given `message` to the service worker.
  // `result_callback` is passed a success boolean indicating whether the
  // message was dispatched successfully.
  virtual void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      const blink::StorageKey& key,
      blink::TransferableMessage message,
      ResultCallback result_callback) = 0;

  // Starts the service worker for `document_url` and `key`. Called when a
  // navigation to that URL is predicted to occur soon.
  virtual void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      const blink::StorageKey& key,
      StartServiceWorkerForNavigationHintCallback callback) = 0;

  // Warms up the service worker for `document_url` and `key`. Called when a
  // navigation to that URL is predicted to occur soon. Unlike
  // StartServiceWorkerForNavigationHint, this function doesn't evaluate the
  // service worker script. Instead, this function prepares renderer process,
  // mojo connections, loading scripts from disk without evaluating the script.
  virtual void WarmUpServiceWorker(const GURL& document_url,
                                   const blink::StorageKey& key,
                                   WarmUpServiceWorkerCallback callback) = 0;

  // Stops all running workers on the given `key`.
  virtual void StopAllServiceWorkersForStorageKey(
      const blink::StorageKey& key) = 0;

  // Stops all running service workers.
  virtual void StopAllServiceWorkers(base::OnceClosure callback) = 0;

  // Gets info about all running workers.
  virtual const base::flat_map<int64_t /* version_id */,
                               ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() = 0;

  // Returns true if the ServiceWorkerVersion for `service_worker_version_id` is
  // live and starting.
  virtual bool IsLiveStartingServiceWorker(
      int64_t service_worker_version_id) = 0;

  // Returns true if the ServiceWorkerVersion for `service_worker_version_id` is
  // live and running.
  virtual bool IsLiveRunningServiceWorker(
      int64_t service_worker_version_id) = 0;

  // Returns the InterfaceProvider for the worker specified by
  // `service_worker_version_id`. The caller can use InterfaceProvider to bind
  // interfaces exposed by the Service Worker. CHECKs if
  // `IsLiveRunningServiceWorker()` returns false.
  virtual service_manager::InterfaceProvider& GetRemoteInterfaces(
      int64_t service_worker_version_id) = 0;

  // Returns the AssociatedInterfaceProvider for the worker specified by
  // `service_worker_version_id`. The caller can use InterfaceProvider to bind
  // interfaces exposed by the Service Worker. CHECKs if
  // `IsLiveRunningServiceWorker()` returns false.
  virtual blink::AssociatedInterfaceProvider& GetRemoteAssociatedInterfaces(
      int64_t service_worker_version_id) = 0;

  // Sets the devtools force update on page load flag for service workers. See
  // ServiceWorkerContextCore::force_update_on_page_load() for details.
  virtual void SetForceUpdateOnPageLoadForTesting(
      bool force_update_on_page_load) = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
