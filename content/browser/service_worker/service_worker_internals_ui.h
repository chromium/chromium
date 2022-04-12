// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace base {
class ListValue;
}

namespace content {

class StoragePartition;
class ServiceWorkerContextWrapper;
struct ServiceWorkerRegistrationInfo;
struct ServiceWorkerVersionInfo;

class ServiceWorkerInternalsUI : public WebUIController {
 public:
  explicit ServiceWorkerInternalsUI(WebUI* web_ui);
  ~ServiceWorkerInternalsUI() override;
};

class ServiceWorkerInternalsHandler : public WebUIMessageHandler {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;

  ServiceWorkerInternalsHandler();
  ~ServiceWorkerInternalsHandler() override;

  void OnRunningStateChanged();
  void OnVersionStateChanged(int partition_id, int64_t version_id);
  void OnErrorEvent(const std::string& event_name,
                    int partition_id,
                    int64_t version_id,
                    base::Value details);
  void OnRegistrationEvent(const std::string& event_name, const GURL& scope);
  void OnDidGetRegistrations(
      int partition_id,
      const base::FilePath& context_path,
      const std::vector<ServiceWorkerRegistrationInfo>& live_registrations,
      const std::vector<ServiceWorkerVersionInfo>& live_versions,
      const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations);
  void OnOperationComplete(int status, const std::string& callback_id);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  class OperationProxy;
  class PartitionObserver;

  void AddContextFromStoragePartition(StoragePartition* partition);

  void RemoveObserverFromStoragePartition(StoragePartition* partition);

  // Called from Javascript.
  void HandleGetOptions(const base::ListValue* args);
  void HandleSetOption(const base::ListValue* args);
  void HandleGetAllRegistrations(const base::ListValue* args);
  void HandleStopWorker(const base::ListValue* args);
  void HandleInspectWorker(const base::ListValue* args);
  void HandleUnregister(const base::ListValue* args);
  void HandleStartWorker(const base::ListValue* args);

  bool GetServiceWorkerContext(
      int partition_id,
      scoped_refptr<ServiceWorkerContextWrapper>* context);
  void FindStoragePartitionById(int partition_id,
                                StoragePartition** result_partition,
                                StoragePartition* storage_partition) const;

  void StopWorkerWithId(scoped_refptr<ServiceWorkerContextWrapper> context,
                        int64_t version_id,
                        StatusCallback callback);
  void UnregisterWithScope(scoped_refptr<ServiceWorkerContextWrapper> context,
                           const GURL& scope,
                           StatusCallback callback) const;

  std::unordered_map<uintptr_t, std::unique_ptr<PartitionObserver>> observers_;
  int next_partition_id_ = 0;
  base::WeakPtrFactory<ServiceWorkerInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
