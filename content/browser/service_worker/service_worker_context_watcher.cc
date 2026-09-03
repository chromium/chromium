// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_watcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

bool IsStoppedAndRedundant(const ServiceWorkerVersionInfo& version_info) {
  return version_info.running_status == blink::EmbeddedWorkerStatus::kStopped &&
         version_info.status == content::ServiceWorkerVersion::REDUNDANT;
}

}  // namespace

ServiceWorkerContextWatcher::ServiceWorkerContextWatcher(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    WorkerRegistrationUpdatedCallback registration_callback,
    WorkerVersionUpdatedCallback version_callback,
    WorkerErrorReportedCallback error_callback)
    : context_(std::move(context)),
      registration_callback_(std::move(registration_callback)),
      version_callback_(std::move(version_callback)),
      error_callback_(std::move(error_callback)) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
}

void ServiceWorkerContextWatcher::Start() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (is_stopped_)
    return;
  context_->GetAllRegistrations(base::BindOnce(
      &ServiceWorkerContextWatcher::OnStoredRegistrations, this));
}

void ServiceWorkerContextWatcher::Stop() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  stop_called_ = true;
  context_->RemoveObserver(this);
  is_stopped_ = true;
}

void ServiceWorkerContextWatcher::OnStoredRegistrations(
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (is_stopped_)
    return;
  context_->AddObserver(this);

  std::unordered_map<int64_t, std::unique_ptr<ServiceWorkerRegistrationInfo>>
      registration_info_map;
  for (const auto& registration : stored_registrations)
    StoreRegistrationInfo(registration, &registration_info_map);
  for (const auto& registration : context_->GetAllLiveRegistrationInfo())
    StoreRegistrationInfo(registration, &registration_info_map);
  for (const auto& version : context_->GetAllLiveVersionInfo())
    StoreVersionInfo(version);

  std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>> registrations =
      std::make_unique<std::vector<ServiceWorkerRegistrationInfo>>();
  registrations->reserve(registration_info_map.size());
  for (const auto& registration_id_info_pair : registration_info_map)
    registrations->push_back(*registration_id_info_pair.second);

  std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions =
      std::make_unique<std::vector<ServiceWorkerVersionInfo>>();
  versions->reserve(version_info_map_.size());

  for (auto version_it = version_info_map_.begin();
       version_it != version_info_map_.end();) {
    versions->push_back(*version_it->second);
    if (IsStoppedAndRedundant(*version_it->second))
      version_info_map_.erase(version_it++);
    else
      ++version_it;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback,
          this, std::move(registrations)));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback, this,
          std::move(versions)));
}

ServiceWorkerContextWatcher::~ServiceWorkerContextWatcher() = default;

void ServiceWorkerContextWatcher::StoreRegistrationInfo(
    const ServiceWorkerRegistrationInfo& registration_info,
    std::unordered_map<int64_t, std::unique_ptr<ServiceWorkerRegistrationInfo>>*
        info_map) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (registration_info.registration_id ==
      blink::mojom::kInvalidServiceWorkerRegistrationId)
    return;
  (*info_map)[registration_info.registration_id] =
      std::make_unique<ServiceWorkerRegistrationInfo>(registration_info);
  StoreVersionInfo(registration_info.active_version);
  StoreVersionInfo(registration_info.waiting_version);
  StoreVersionInfo(registration_info.installing_version);
}

void ServiceWorkerContextWatcher::StoreVersionInfo(
    const ServiceWorkerVersionInfo& version_info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (version_info.version_id == blink::mojom::kInvalidServiceWorkerVersionId)
    return;
  version_info_map_[version_info.version_id] =
      std::make_unique<ServiceWorkerVersionInfo>(version_info);
}

void ServiceWorkerContextWatcher::SendRegistrationInfo(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerRegistrationInfo::DeleteFlag delete_flag) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>> registrations =
      std::make_unique<std::vector<ServiceWorkerRegistrationInfo>>();
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    registrations->push_back(registration->GetInfo());
  } else {
    registrations->push_back(ServiceWorkerRegistrationInfo(
        scope, key, registration_id, delete_flag));
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback,
          this, std::move(registrations)));
}

void ServiceWorkerContextWatcher::SendVersionInfo(
    const ServiceWorkerVersionInfo& version_info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions =
      std::make_unique<std::vector<ServiceWorkerVersionInfo>>();
  versions->push_back(version_info);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback, this,
          std::move(versions)));
}

void ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback(
    std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>> registrations) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (stop_called_)
    return;
  registration_callback_.Run(*registrations.get());
}

void ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback(
    std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (stop_called_)
    return;
  version_callback_.Run(*versions.get());
}

void ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback(
    int64_t registration_id,
    int64_t version_id,
    std::unique_ptr<ServiceWorkerContextObserver::ErrorInfo> error_info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (stop_called_)
    return;
  error_callback_.Run(registration_id, version_id, *error_info.get());
}

void ServiceWorkerContextWatcher::OnNewLiveRegistration(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  SendRegistrationInfo(registration_id, scope, key,
                       ServiceWorkerRegistrationInfo::IS_NOT_DELETED);
}

void ServiceWorkerContextWatcher::OnNewLiveVersion(
    const ServiceWorkerVersionInfo& version_info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  int64_t version_id = version_info.version_id;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end()) {
    CHECK_EQ(it->second->registration_id, version_info.registration_id,
             base::NotFatalUntil::M159);
    CHECK_EQ(it->second->script_url, version_info.script_url,
             base::NotFatalUntil::M159);
    return;
  }

  std::unique_ptr<ServiceWorkerVersionInfo> version =
      std::make_unique<ServiceWorkerVersionInfo>(version_info);
  SendVersionInfo(*version);
  if (!IsStoppedAndRedundant(*version))
    version_info_map_[version_id] = std::move(version);
}

void ServiceWorkerContextWatcher::OnStarting(int64_t version_id) {
  OnRunningStateChanged(version_id, blink::EmbeddedWorkerStatus::kStarting);
}

void ServiceWorkerContextWatcher::OnStarted(
    int64_t version_id,
    const GURL& scope,
    ChildProcessId process_id,
    const GURL& script_url,
    const blink::ServiceWorkerToken& token,
    const blink::StorageKey& key) {
  OnRunningStateChanged(version_id, blink::EmbeddedWorkerStatus::kRunning);
}

void ServiceWorkerContextWatcher::OnStopping(int64_t version_id) {
  OnRunningStateChanged(version_id, blink::EmbeddedWorkerStatus::kStopping);
}

void ServiceWorkerContextWatcher::OnStopped(int64_t version_id) {
  OnRunningStateChanged(version_id, blink::EmbeddedWorkerStatus::kStopped);
}

void ServiceWorkerContextWatcher::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    content::ServiceWorkerVersion::Status status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  if (version->status == status)
    return;
  version->status = status;
  SendVersionInfo(*version);
  if (IsStoppedAndRedundant(*version))
    version_info_map_.erase(version_id);
}

void ServiceWorkerContextWatcher::OnVersionRouterRulesChanged(
    int64_t version_id,
    const ServiceWorkerVersion::RouterRulesForDevTools& router_rules) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end()) {
    return;
  }
  ServiceWorkerVersionInfo* version = it->second.get();
  version->router_rules = router_rules.legacy_rules;
  version->typed_router_rules = router_rules.typed_rules;
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnVersionDevToolsRoutingIdChanged(
    int64_t version_id,
    ChildProcessId process_id,
    int devtools_agent_route_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  if (version->process_id == process_id &&
      version->devtools_agent_route_id == devtools_agent_route_id) {
    return;
  }
  version->process_id = process_id;
  version->devtools_agent_route_id = devtools_agent_route_id;
  SendVersionInfo(*version);
  if (IsStoppedAndRedundant(*version))
    version_info_map_.erase(version_id);
}

void ServiceWorkerContextWatcher::OnMainScriptResponseSet(
    int64_t version_id,
    base::Time script_response_time,
    base::Time script_last_modified) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  version->script_response_time = script_response_time;
  version->script_last_modified = script_last_modified;
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnErrorReported(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end())
    registration_id = it->second->registration_id;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback, this,
          registration_id, version_id,
          std::make_unique<ServiceWorkerContextObserver::ErrorInfo>(info)));
}

void ServiceWorkerContextWatcher::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const ConsoleMessage& message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (message.message_level != blink::mojom::ConsoleMessageLevel::kError)
    return;
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end())
    registration_id = it->second->registration_id;

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback, this,
          registration_id, version_id,
          std::make_unique<ServiceWorkerContextObserver::ErrorInfo>(
              message.message, message.line_number, -1, message.source_url)));
}

void ServiceWorkerContextWatcher::OnControlleeAdded(
    int64_t version_id,
    const std::string& uuid,
    const ServiceWorkerClientInfo& info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();

  version->clients.insert_or_assign(uuid, info);

  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnControlleeRemoved(int64_t version_id,
                                                      const std::string& uuid) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  version->clients.erase(uuid);
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnRegistrationCompleted(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  SendRegistrationInfo(registration_id, scope, key,
                       ServiceWorkerRegistrationInfo::IS_NOT_DELETED);
}

void ServiceWorkerContextWatcher::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  SendRegistrationInfo(registration_id, scope, key,
                       ServiceWorkerRegistrationInfo::IS_DELETED);
}

void ServiceWorkerContextWatcher::OnRunningStateChanged(
    int64_t version_id,
    blink::EmbeddedWorkerStatus running_status) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  if (version->running_status == running_status)
    return;
  version->running_status = running_status;
  SendVersionInfo(*version);
  if (IsStoppedAndRedundant(*version))
    version_info_map_.erase(version_id);
}

}  // namespace content
