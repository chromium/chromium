// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_watcher.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

bool IsStoppedAndRedundant(const ServiceWorkerVersionInfo& version_info) {
  return version_info.running_status == EmbeddedWorkerStatus::STOPPED &&
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerContextWatcher::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWatcher::GetStoredRegistrationsOnCoreThread,
          this));
}

void ServiceWorkerContextWatcher::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  stop_called_ = true;
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWatcher::StopOnCoreThread, this));
}

void ServiceWorkerContextWatcher::GetStoredRegistrationsOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (is_stopped_)
    return;
  context_->GetAllRegistrations(base::BindOnce(
      &ServiceWorkerContextWatcher::OnStoredRegistrationsOnCoreThread, this));
}

void ServiceWorkerContextWatcher::OnStoredRegistrationsOnCoreThread(
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback,
          this, std::move(registrations)));
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback, this,
          std::move(versions)));
}

void ServiceWorkerContextWatcher::StopOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  context_->RemoveObserver(this);
  is_stopped_ = true;
}

ServiceWorkerContextWatcher::~ServiceWorkerContextWatcher() {
}

void ServiceWorkerContextWatcher::StoreRegistrationInfo(
    const ServiceWorkerRegistrationInfo& registration_info,
    std::unordered_map<int64_t, std::unique_ptr<ServiceWorkerRegistrationInfo>>*
        info_map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (version_info.version_id == blink::mojom::kInvalidServiceWorkerVersionId)
    return;
  version_info_map_[version_info.version_id] =
      std::make_unique<ServiceWorkerVersionInfo>(version_info);
}

void ServiceWorkerContextWatcher::SendRegistrationInfo(
    int64_t registration_id,
    const GURL& scope,
    ServiceWorkerRegistrationInfo::DeleteFlag delete_flag) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>> registrations =
      std::make_unique<std::vector<ServiceWorkerRegistrationInfo>>();
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    registrations->push_back(registration->GetInfo());
  } else {
    registrations->push_back(
        ServiceWorkerRegistrationInfo(scope, registration_id, delete_flag));
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback,
          this, std::move(registrations)));
}

void ServiceWorkerContextWatcher::SendVersionInfo(
    const ServiceWorkerVersionInfo& version_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions =
      std::make_unique<std::vector<ServiceWorkerVersionInfo>>();
  versions->push_back(version_info);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback, this,
          std::move(versions)));
}

void ServiceWorkerContextWatcher::RunWorkerRegistrationUpdatedCallback(
    std::unique_ptr<std::vector<ServiceWorkerRegistrationInfo>> registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (stop_called_)
    return;
  registration_callback_.Run(*registrations.get());
}

void ServiceWorkerContextWatcher::RunWorkerVersionUpdatedCallback(
    std::unique_ptr<std::vector<ServiceWorkerVersionInfo>> versions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (stop_called_)
    return;
  version_callback_.Run(*versions.get());
}

void ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback(
    int64_t registration_id,
    int64_t version_id,
    std::unique_ptr<ErrorInfo> error_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (stop_called_)
    return;
  error_callback_.Run(registration_id, version_id, *error_info.get());
}

void ServiceWorkerContextWatcher::OnNewLiveRegistration(int64_t registration_id,
                                                        const GURL& scope) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  SendRegistrationInfo(registration_id, scope,
                       ServiceWorkerRegistrationInfo::IS_NOT_DELETED);
}

void ServiceWorkerContextWatcher::OnNewLiveVersion(
    const ServiceWorkerVersionInfo& version_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  int64_t version_id = version_info.version_id;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end()) {
    DCHECK_EQ(it->second->registration_id, version_info.registration_id);
    DCHECK_EQ(it->second->script_url, version_info.script_url);
    return;
  }

  std::unique_ptr<ServiceWorkerVersionInfo> version =
      std::make_unique<ServiceWorkerVersionInfo>(version_info);
  SendVersionInfo(*version);
  if (!IsStoppedAndRedundant(*version))
    version_info_map_[version_id] = std::move(version);
}

void ServiceWorkerContextWatcher::OnStarting(int64_t version_id) {
  OnRunningStateChanged(version_id, EmbeddedWorkerStatus::STARTING);
}

void ServiceWorkerContextWatcher::OnStarted(int64_t version_id,
                                            const GURL& scope,
                                            int process_id,
                                            const GURL& script_url) {
  OnRunningStateChanged(version_id, EmbeddedWorkerStatus::RUNNING);
}

void ServiceWorkerContextWatcher::OnStopping(int64_t version_id) {
  OnRunningStateChanged(version_id, EmbeddedWorkerStatus::STOPPING);
}

void ServiceWorkerContextWatcher::OnStopped(int64_t version_id) {
  OnRunningStateChanged(version_id, EmbeddedWorkerStatus::STOPPED);
}

void ServiceWorkerContextWatcher::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    content::ServiceWorkerVersion::Status status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

void ServiceWorkerContextWatcher::OnVersionDevToolsRoutingIdChanged(
    int64_t version_id,
    int process_id,
    int devtools_agent_route_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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

void ServiceWorkerContextWatcher::OnMainScriptHttpResponseInfoSet(
    int64_t version_id,
    base::Time script_response_time,
    base::Time script_last_modified) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  version->script_response_time = script_response_time;
  version->script_last_modified = script_last_modified;
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnErrorReported(int64_t version_id,
                                                  const ErrorInfo& info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end())
    registration_id = it->second->registration_id;
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback, this,
          registration_id, version_id, std::make_unique<ErrorInfo>(info)));
}

void ServiceWorkerContextWatcher::OnReportConsoleMessage(
    int64_t version_id,
    const ConsoleMessage& message) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (message.message_level != blink::mojom::ConsoleMessageLevel::kError)
    return;
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  auto it = version_info_map_.find(version_id);
  if (it != version_info_map_.end())
    registration_id = it->second->registration_id;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerContextWatcher::RunWorkerErrorReportedCallback, this,
          registration_id, version_id,
          std::make_unique<ErrorInfo>(message.message, message.line_number, -1,
                                      message.source_url)));
}

void ServiceWorkerContextWatcher::OnControlleeAdded(
    int64_t version_id,
    const GURL& scope,
    const std::string& uuid,
    const ServiceWorkerClientInfo& info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  version->clients[uuid] = info;
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnControlleeRemoved(int64_t version_id,
                                                      const GURL& scope,
                                                      const std::string& uuid) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  auto it = version_info_map_.find(version_id);
  if (it == version_info_map_.end())
    return;
  ServiceWorkerVersionInfo* version = it->second.get();
  version->clients.erase(uuid);
  SendVersionInfo(*version);
}

void ServiceWorkerContextWatcher::OnRegistrationCompleted(
    int64_t registration_id,
    const GURL& scope) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  SendRegistrationInfo(registration_id, scope,
                       ServiceWorkerRegistrationInfo::IS_NOT_DELETED);
}

void ServiceWorkerContextWatcher::OnRegistrationDeleted(int64_t registration_id,
                                                        const GURL& scope) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  SendRegistrationInfo(registration_id, scope,
                       ServiceWorkerRegistrationInfo::IS_DELETED);
}

void ServiceWorkerContextWatcher::OnRunningStateChanged(
    int64_t version_id,
    EmbeddedWorkerStatus running_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
