// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class StoragePartition;
class ServiceWorkerInternalsUI;
class ServiceWorkerContextWrapper;
struct ServiceWorkerRegistrationInfo;
struct ServiceWorkerVersionInfo;

class ServiceWorkerInternalsUIConfig
    : public DefaultWebUIConfig<ServiceWorkerInternalsUI> {
 public:
  ServiceWorkerInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme,
                           kChromeUIServiceWorkerInternalsHost) {}
};

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
  void OnVersionRouterRulesChanged();
  void OnErrorEvent(const std::string& event_name,
                    int partition_id,
                    int64_t version_id,
                    const base::Value::Dict& details);
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
  void HandleGetOptions(const base::Value::List& args);
  void HandleSetOption(const base::Value::List& args);
  void HandleGetAllRegistrations(const base::Value::List& args);
  void HandleStopWorker(const base::Value::List& args);
  void HandleInspectWorker(const base::Value::List& args);
  void HandleUnregister(const base::Value::List& args);
  void HandleStartWorker(const base::Value::List& args);

  bool GetServiceWorkerContext(
      int partition_id,
      scoped_refptr<ServiceWorkerContextWrapper>* context);

  void StopWorkerWithId(scoped_refptr<ServiceWorkerContextWrapper> context,
                        int64_t version_id,
                        StatusCallback callback);
  void UnregisterWithScope(scoped_refptr<ServiceWorkerContextWrapper> context,
                           const GURL& scope,
                           blink::StorageKey& storage_key,
                           StatusCallback callback) const;

  std::unordered_map<uintptr_t, std::unique_ptr<PartitionObserver>> observers_;
  int next_partition_id_ = 0;
  base::WeakPtrFactory<ServiceWorkerInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
