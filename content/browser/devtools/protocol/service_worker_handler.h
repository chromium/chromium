// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_

#include <stdint.h>

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/service_worker.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_info.h"

namespace content {

class BrowserContext;
class RenderFrameHostImpl;
class ServiceWorkerContextWatcher;
class ServiceWorkerContextWrapper;
class StoragePartitionImpl;

namespace protocol {

class ServiceWorkerHandler : public DevToolsDomainHandler,
                             public ServiceWorker::Backend {
 public:
  explicit ServiceWorkerHandler(bool allow_inspect_worker);

  ServiceWorkerHandler(const ServiceWorkerHandler&) = delete;
  ServiceWorkerHandler& operator=(const ServiceWorkerHandler&) = delete;

  ~ServiceWorkerHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  Response Enable() override;
  Response Disable() override;
  Response Unregister(const std::string& scope_url) override;
  Response StartWorker(const std::string& scope_url) override;
  Response SkipWaiting(const std::string& scope_url) override;
  Response StopWorker(const std::string& version_id) override;
  void StopAllWorkers(
      std::unique_ptr<StopAllWorkersCallback> callback) override;
  Response UpdateRegistration(const std::string& scope_url) override;
  Response InspectWorker(const std::string& version_id) override;
  Response SetForceUpdateOnPageLoad(bool force_update_on_page_load) override;
  Response DeliverPushMessage(const std::string& origin,
                              const std::string& registration_id,
                              const std::string& data) override;
  Response DispatchSyncEvent(const std::string& origin,
                             const std::string& registration_id,
                             const std::string& tag,
                             bool last_chance) override;
  Response DispatchPeriodicSyncEvent(const std::string& origin,
                                     const std::string& registration_id,
                                     const std::string& tag) override;

 private:
  void OnWorkerRegistrationUpdated(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);
  void OnWorkerVersionUpdated(
      const std::vector<ServiceWorkerVersionInfo>& registrations);
  void OnErrorReported(int64_t registration_id,
                       int64_t version_id,
                       const ServiceWorkerContextObserver::ErrorInfo& info);

  void OpenNewDevToolsWindow(int process_id, int devtools_agent_route_id);
  void ClearForceUpdate();

  const bool allow_inspect_worker_;
  scoped_refptr<ServiceWorkerContextWrapper> context_;
  std::unique_ptr<ServiceWorker::Frontend> frontend_;
  bool enabled_;
  scoped_refptr<ServiceWorkerContextWatcher> context_watcher_;
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<StoragePartitionImpl> storage_partition_;

  base::WeakPtrFactory<ServiceWorkerHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
