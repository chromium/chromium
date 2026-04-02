// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_host/service_process_tracker.h"

#include <map>
#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "url/gurl.h"

namespace content {

ServiceProcessTracker::ServiceProcessTracker() = default;

ServiceProcessTracker::~ServiceProcessTracker() = default;

ServiceProcessInfo ServiceProcessTracker::AddProcess(
    base::Process process,
    const std::optional<GURL>& site,
    const std::string& service_interface_name,
    base::WeakPtr<ServiceProcessHost::Observer> observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto id = GenerateNextId();
  ServiceProcessInfo info(service_interface_name, site, id, std::move(process));
  auto info_dup = info.Duplicate();
  processes_.insert({id, std::move(info)});
  instance_observers_.insert({id, observer});

  for (auto& obs : observers_) {
    obs.OnServiceProcessLaunched(info_dup);
  }
  if (observer) {
    observer->OnServiceProcessLaunched(info_dup);
  }
  return info_dup;
}

void ServiceProcessTracker::NotifyTerminated(ServiceProcessId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = processes_.find(id);
  CHECK(iter != processes_.end());

  auto info_dup = iter->second.Duplicate();
  for (auto& obs : observers_) {
    obs.OnServiceProcessTerminatedNormally(info_dup);
  }

  auto obs_iter = instance_observers_.find(id);
  if (obs_iter != instance_observers_.end()) {
    if (obs_iter->second) {
      obs_iter->second->OnServiceProcessTerminatedNormally(info_dup);
    }
    instance_observers_.erase(obs_iter);
  }

  processes_.erase(iter);
}

void ServiceProcessTracker::NotifyCrashed(
    ServiceProcessId id,
    UtilityProcessHost::Client::CrashType crash_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = processes_.find(id);
  CHECK(iter != processes_.end());

  switch (crash_type) {
    case UtilityProcessHost::Client::CrashType::kPreIpcInitialization:
      iter->second.set_crashed_pre_ipc(true);
      break;
    case UtilityProcessHost::Client::CrashType::kPostIpcInitialization:
      iter->second.set_crashed_pre_ipc(false);
      break;
  }

  auto info_dup = iter->second.Duplicate();
  for (auto& obs : observers_) {
    obs.OnServiceProcessCrashed(info_dup);
  }

  auto obs_iter = instance_observers_.find(id);
  if (obs_iter != instance_observers_.end()) {
    if (obs_iter->second) {
      obs_iter->second->OnServiceProcessCrashed(info_dup);
    }
    instance_observers_.erase(obs_iter);
  }

  processes_.erase(iter);
}

void ServiceProcessTracker::AddObserver(
    ServiceProcessHost::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void ServiceProcessTracker::RemoveObserver(
    ServiceProcessHost::Observer* observer) {
  // NOTE: Some tests may remove observers after BrowserThreads are shut down.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void ServiceProcessTracker::ClearInstanceObserver(
    ServiceProcessHost::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& [id, obs] : instance_observers_) {
    if (obs.get() == observer) {
      obs.reset();
    }
  }
}

std::vector<ServiceProcessInfo> ServiceProcessTracker::GetProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<ServiceProcessInfo> processes;
  for (const auto& entry : processes_) {
    processes.push_back(entry.second.Duplicate());
  }
  return processes;
}

ServiceProcessId ServiceProcessTracker::GenerateNextId() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return service_process_id_generator_.GenerateNextId();
}

ServiceProcessTracker& GetServiceProcessTracker() {
  static base::NoDestructor<ServiceProcessTracker> tracker;
  return *tracker;
}

}  // namespace content
