// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/browser/service_worker_running_info.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom-forward.h"
#include "url/gurl.h"

namespace blink {

struct TransferableMessage;

}

namespace content {

class ServiceWorkerContextObserver;
struct StorageUsageInfo;

enum class ServiceWorkerCapability {
  NO_SERVICE_WORKER,
  SERVICE_WORKER_NO_FETCH_HANDLER,
  SERVICE_WORKER_WITH_FETCH_HANDLER,
};

enum class OfflineCapability {
  kUnsupported,
  kSupported,
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

// Represents the per-StoragePartition service worker data.
//
// See service_worker_context_wrapper.cc for the implementation
// of ServiceWorkerContext and ServiceWorkerContextWrapper (the
// primary implementation of this abstract class).
class CONTENT_EXPORT ServiceWorkerContext {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  using GetInstalledRegistrationOriginsCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;

  using GetUsageInfoCallback =
      base::OnceCallback<void(const std::vector<StorageUsageInfo>& usage_info)>;

  using CheckHasServiceWorkerCallback =
      base::OnceCallback<void(ServiceWorkerCapability capability)>;

  using CheckOfflineCapabilityCallback =
      base::OnceCallback<void(OfflineCapability capability)>;

  using CountExternalRequestsCallback =
      base::OnceCallback<void(size_t external_request_count)>;

  using StartServiceWorkerForNavigationHintCallback = base::OnceCallback<void(
      StartServiceWorkerForNavigationHintResult result)>;

  using StartWorkerCallback = base::OnceCallback<
      void(int64_t version_id, int process_id, int thread_id)>;

  // Temporary for crbug.com/824858. The thread the context core lives on.
  static bool IsServiceWorkerOnUIEnabled();
  static content::BrowserThread::ID GetCoreThreadId();

  // Returns true if |url| is within the service worker |scope|.
  static bool ScopeMatches(const GURL& scope, const GURL& url);

  // Runs a |task| on task |runner| making sure that
  // |service_worker_context| is alive while the task is being run.
  static void RunTask(scoped_refptr<base::SequencedTaskRunner> runner,
                      const base::Location& from_here,
                      ServiceWorkerContext* service_worker_context,
                      base::OnceClosure task);

  // Observer methods are always dispatched on the UI thread.
  virtual void AddObserver(ServiceWorkerContextObserver* observer) = 0;
  virtual void RemoveObserver(ServiceWorkerContextObserver* observer) = 0;

  // Equivalent to calling navigator.serviceWorker.register(script_url,
  // options). |callback| is passed true when the JS promise is fulfilled or
  // false when the JS promise is rejected.
  //
  // The registration can fail if:
  //  * |script_url| is on a different origin from |scope|
  //  * Fetching |script_url| fails.
  //  * |script_url| fails to parse or its top-level execution fails.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      ResultCallback callback) = 0;

  // Equivalent to calling ServiceWorkerRegistration#unregister on the
  // registration for |scope|. |callback| is passed true when the JS promise is
  // fulfilled or false when the JS promise is rejected.
  //
  // Unregistration can fail if:
  //  * No registration exists for |scope|.
  //  * Something unexpected goes wrong, like a renderer crash.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void UnregisterServiceWorker(const GURL& scope,
                                       ResultCallback callback) = 0;

  // Mechanism for embedder to increment/decrement ref count of a service
  // worker.
  //
  // Embedders can call StartingExternalRequest() while it is performing some
  // work with the worker. The worker is considered to be working until embedder
  // calls FinishedExternalRequest(). This ensures that content/ does not
  // shut the worker down while embedder is expecting the worker to be kept
  // alive.
  //
  // Must be called from the core thread.
  virtual ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) = 0;
  virtual ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) = 0;

  // Returns the pending external request count for the worker with the
  // specified |origin| via |callback|. Must be called from the UI thread. The
  // callback is called on the UI thread.
  virtual void CountExternalRequestsForTest(
      const url::Origin& origin,
      CountExternalRequestsCallback callback) = 0;

  // Whether |origin| has any registrations. Uninstalling and uninstalled
  // registrations do not cause this to return true, that is, only registrations
  // with status ServiceWorkerRegistration::Status::kIntact are considered, such
  // as even if the corresponding live registrations may still exist. Also,
  // returns true if it doesn't know (registrations are not yet initialized).
  // Must be called on the UI thread.
  virtual bool MaybeHasRegistrationForOrigin(const url::Origin& origin) = 0;

  // Returns a set of origins which have at least one stored registration.
  // The set doesn't include installing/uninstalling/uninstalled registrations.
  // When |host_filter| is specified the set only includes origins whose host
  // matches |host_filter|.
  // This function can be called from any thread and the callback is called on
  // that thread.
  virtual void GetInstalledRegistrationOrigins(
      base::Optional<std::string> host_filter,
      GetInstalledRegistrationOriginsCallback callback) = 0;

  // May be called from any thread, and the callback is called on that thread.
  virtual void GetAllOriginsInfo(GetUsageInfoCallback callback) = 0;

  // This function can be called from any thread, and the callback is called
  // on that thread.  Deletes all registrations in the origin and clears all
  // service workers belonging to the registrations. All clients controlled by
  // those service workers will lose their controllers immediately after this
  // operation.
  virtual void DeleteForOrigin(const url::Origin& origin_url,
                               ResultCallback callback) = 0;

  // Performs internal storage cleanup. Operations to the storage in the past
  // (e.g. deletion) are usually recorded in disk for a certain period until
  // compaction happens. This method wipes them out to ensure that the deleted
  // entries and other traces like log files are removed.
  // May be called on any thread, and the callback is called on that thread.
  virtual void PerformStorageCleanup(base::OnceClosure callback) = 0;

  // Returns ServiceWorkerCapability describing existence and properties of a
  // Service Worker registration matching |url|. In case the service
  // worker is being installed as of calling this method, it will wait for the
  // installation to finish before coming back with the result.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void CheckHasServiceWorker(
      const GURL& url,
      CheckHasServiceWorkerCallback callback) = 0;

  // Simulates a navigation request in the offline state and dispatches a fetch
  // event. Returns OfflineCapability::kSupported if the response's status code
  // is 200.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  //
  // TODO(hayato): Re-visit to integrate this function with
  // |ServiceWorkerContext::CheckHasServiceWorker|.
  virtual void CheckOfflineCapability(
      const GURL& url,
      CheckOfflineCapabilityCallback callback) = 0;

  // Stops all running service workers and unregisters all service worker
  // registrations. This method is used in web tests to make sure that the
  // existing service worker will not affect the succeeding tests.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void ClearAllServiceWorkersForTest(base::OnceClosure callback) = 0;

  // Starts the active worker of the registration for the given |scope|. If
  // there is no active worker, starts the installing worker.
  // |info_callback| is passed information about the started worker if
  // successful, otherwise |failure_callback| is called.
  //
  // Must be called on the core thread, and the callback is called on that
  // thread. There is no guarantee about whether the callback is called
  // synchronously or asynchronously.
  virtual void StartWorkerForScope(const GURL& scope,
                                   StartWorkerCallback info_callback,
                                   base::OnceClosure failure_callback) = 0;

  // Starts the active worker of the registration for the given |scope| and
  // dispatches the given |message| to the service worker. |result_callback|
  // is passed a success boolean indicating whether the message was dispatched
  // successfully.
  //
  // May be called on any thread, and the callback is called on that thread.
  virtual void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      blink::TransferableMessage message,
      ResultCallback result_callback) = 0;

  // Starts the service worker for |document_url|. Called when a navigation to
  // that URL is predicted to occur soon. Must be called from the UI thread. The
  // |callback| will always be called on the UI thread.
  virtual void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback) = 0;

  // Stops all running workers on the given |origin|.
  //
  // This function can be called from any thread.
  virtual void StopAllServiceWorkersForOrigin(const url::Origin& origin) = 0;

  // Stops all running service workers.
  //
  // This function can be called from any thread.
  // |callback| is called on the caller's thread.
  virtual void StopAllServiceWorkers(base::OnceClosure callback) = 0;

  // Gets info about all running workers.
  //
  // Must be called on the UI thread. The callback is called on the UI thread.
  virtual const base::flat_map<int64_t /* version_id */,
                               ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
