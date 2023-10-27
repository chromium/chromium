// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class ServiceWorkerContextWrapper;

// Used to monitor the status change of the ServiceWorker registrations and
// versions in the ServiceWorkerContext from UI thread.
class CONTENT_EXPORT ServiceWorkerContextWatcher
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<ServiceWorkerContextWatcher> {
 public:
  using WorkerRegistrationUpdatedCallback = base::RepeatingCallback<void(
      const std::vector<ServiceWorkerRegistrationInfo>&)>;
  using WorkerVersionUpdatedCallback = base::RepeatingCallback<void(
      const std::vector<ServiceWorkerVersionInfo>&)>;
  using WorkerErrorReportedCallback = base::RepeatingCallback<void(
      int64_t /* registration_id */,
      int64_t /* version_id */,
      const ServiceWorkerContextObserver::ErrorInfo&)>;

  ServiceWorkerContextWatcher(
      scoped_refptr<ServiceWorkerContextWrapper> context,
      WorkerRegistrationUpdatedCallback registration_callback,
      WorkerVersionUpdatedCallback version_callback,
      WorkerErrorReportedCallback error_callback);
  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWatcher>;
  friend class ServiceWorkerContextWatcherTest;

  ~ServiceWorkerContextWatcher() override;

  void OnStoredRegistrations(
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations);

  void StoreRegistrationInfo(
      const ServiceWorkerRegistrationInfo& registration,
      std::unordered_map<int64_t,
                         std::unique_ptr<ServiceWorkerRegistrationInfo>>*
          info_map);
  void StoreVersionInfo(const ServiceWorkerVersionInfo& version);

  void SendRegistrationInfo(
      int64_t registration_id,
      const GURL& scope,
      const blink::StorageKey& key,
      ServiceWorkerRegistrationInfo::DeleteFlag delete_flag);
  void SendVersionInfo(const ServiceWorkerVersionInfo& version);

  void RunWorkerRegistrationUpdatedCallback(
      std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>>
          registrations);
  void RunWorkerVersionUpdatedCallback(
      std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions);
  void RunWorkerErrorReportedCallback(
      int64_t registration_id,
      int64_t version_id,
      std::unique_ptr<ServiceWorkerContextObserver::ErrorInfo> error_info);

  // ServiceWorkerContextCoreObserver implements
  void OnNewLiveRegistration(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override;
  void OnNewLiveVersion(const ServiceWorkerVersionInfo& version_info) override;
  void OnStarting(int64_t version_id) override;
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url,
                 const blink::ServiceWorkerToken& token,
                 const blink::StorageKey& key) override;
  void OnStopping(int64_t version_id) override;
  void OnStopped(int64_t version_id) override;
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status status) override;
  void OnVersionRouterRulesChanged(int64_t version_id,
                                   const std::string& router_rules) override;
  void OnVersionDevToolsRoutingIdChanged(int64_t version_id,
                                         int process_id,
                                         int devtools_agent_route_id) override;
  void OnMainScriptResponseSet(int64_t version_id,
                               base::Time script_response_time,
                               base::Time script_last_modified) override;
  void OnErrorReported(
      int64_t version_id,
      const GURL& scope,
      const blink::StorageKey& key,
      const ServiceWorkerContextObserver::ErrorInfo& info) override;
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const blink::StorageKey& key,
                              const ConsoleMessage& message) override;
  void OnControlleeAdded(int64_t version_id,
                         const std::string& uuid,
                         const ServiceWorkerClientInfo& info) override;
  void OnControlleeRemoved(int64_t version_id,
                           const std::string& uuid) override;
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope,
                               const blink::StorageKey& key) override;
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override;

  void OnRunningStateChanged(int64_t version_id,
                             blink::EmbeddedWorkerStatus running_status);

  std::unordered_map<int64_t, std::unique_ptr<ServiceWorkerVersionInfo>>
      version_info_map_;
  scoped_refptr<ServiceWorkerContextWrapper> context_;
  WorkerRegistrationUpdatedCallback registration_callback_;
  WorkerVersionUpdatedCallback version_callback_;
  WorkerErrorReportedCallback error_callback_;
  // Should be used on UI thread only.
  bool stop_called_ = false;
  // Should be used on UI thread only.
  bool is_stopped_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_
