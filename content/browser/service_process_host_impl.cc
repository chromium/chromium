// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "content/browser/utility_process_host.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

namespace {

// Internal helper to track running service processes. Usage of this class is
// split across the IO thread and UI thread.
class ServiceProcessTracker {
 public:
  ServiceProcessTracker()
      : ui_task_runner_(
            base::CreateSingleThreadTaskRunner({BrowserThread::UI})) {}
  ~ServiceProcessTracker() = default;

  ServiceProcessInfo AddProcess(const base::Process& process,
                                const std::string& service_interface_name) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::AutoLock lock(processes_lock_);
    auto id = GenerateNextId();
    ServiceProcessInfo& info = processes_[id];
    info.service_process_id = id;
    info.pid = process.Pid();
    info.service_interface_name = service_interface_name;
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceProcessTracker::NotifyLaunchOnUIThread,
                       base::Unretained(this), info));
    return info;
  }

  void NotifyTerminated(ServiceProcessId id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::AutoLock lock(processes_lock_);
    auto iter = processes_.find(id);
    DCHECK(iter != processes_.end());
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceProcessTracker::NotifyTerminatedOnUIThread,
                       base::Unretained(this), iter->second));
    processes_.erase(iter);
  }

  void NotifyCrashed(ServiceProcessId id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::AutoLock lock(processes_lock_);
    auto iter = processes_.find(id);
    DCHECK(iter != processes_.end());
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceProcessTracker::NotifyCrashedOnUIThread,
                       base::Unretained(this), iter->second));
    processes_.erase(iter);
  }

  void AddObserver(ServiceProcessHost::Observer* observer) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ServiceProcessHost::Observer* observer) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observers_.RemoveObserver(observer);
  }

  std::vector<ServiceProcessInfo> GetProcesses() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::AutoLock lock(processes_lock_);
    std::vector<ServiceProcessInfo> processes;
    for (const auto& entry : processes_)
      processes.push_back(entry.second);
    return processes;
  }

 private:
  void NotifyLaunchOnUIThread(const content::ServiceProcessInfo& info) {
    for (auto& observer : observers_)
      observer.OnServiceProcessLaunched(info);
  }

  void NotifyTerminatedOnUIThread(const content::ServiceProcessInfo& info) {
    for (auto& observer : observers_)
      observer.OnServiceProcessTerminatedNormally(info);
  }

  void NotifyCrashedOnUIThread(const content::ServiceProcessInfo& info) {
    for (auto& observer : observers_)
      observer.OnServiceProcessCrashed(info);
  }

  ServiceProcessId GenerateNextId() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    auto id = next_id_;
    next_id_ = ServiceProcessId::FromUnsafeValue(next_id_.GetUnsafeValue() + 1);
    return id;
  }

  const scoped_refptr<base::TaskRunner> ui_task_runner_;
  ServiceProcessId next_id_{1};

  base::Lock processes_lock_;
  std::map<ServiceProcessId, ServiceProcessInfo> processes_;

  // Observers are owned and used exclusively on the UI thread.
  base::ObserverList<ServiceProcessHost::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessTracker);
};

ServiceProcessTracker& GetServiceProcessTracker() {
  static base::NoDestructor<ServiceProcessTracker> tracker;
  return *tracker;
}

// Helper to bridge UtilityProcessHost IO thread events to the
// ServiceProcessTracker. Every UtilityProcessHost created for a service process
// has a unique instance of this class associated with it.
class UtilityProcessClient : public UtilityProcessHost::Client {
 public:
  explicit UtilityProcessClient(const std::string& service_interface_name)
      : service_interface_name_(service_interface_name) {}
  ~UtilityProcessClient() override = default;

  // UtilityProcessHost::Client:
  void OnProcessLaunched(const base::Process& process) override {
    process_info_ =
        GetServiceProcessTracker().AddProcess(process, service_interface_name_);
  }

  void OnProcessTerminatedNormally() override {
    GetServiceProcessTracker().NotifyTerminated(
        process_info_->service_process_id);
  }

  void OnProcessCrashed() override {
    // TODO(https://crbug.com/1016027): It is unclear how we can observe
    // |OnProcessCrashed()| without observing |OnProcessLaunched()| first, but
    // it can happen on Android. Ignore the notification in this case.
    if (!process_info_)
      return;

    GetServiceProcessTracker().NotifyCrashed(process_info_->service_process_id);
  }

 private:
  const std::string service_interface_name_;
  base::Optional<ServiceProcessInfo> process_info_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessClient);
};

// TODO(crbug.com/977637): Once UtilityProcessHost is used only by service
// processes, its logic can be inlined here.
void LaunchServiceProcessOnIOThread(mojo::GenericPendingReceiver receiver,
                                    ServiceProcessHost::Options options) {
  UtilityProcessHost* host = new UtilityProcessHost(
      std::make_unique<UtilityProcessClient>(*receiver.interface_name()));
  host->SetName(!options.display_name.empty()
                    ? options.display_name
                    : base::UTF8ToUTF16(*receiver.interface_name()));
  host->SetMetricsName(*receiver.interface_name());
  host->SetSandboxType(options.sandbox_type);
  host->SetExtraCommandLineSwitches(std::move(options.extra_switches));
  if (options.child_flags)
    host->set_child_flags(*options.child_flags);
  host->Start();
  host->GetChildProcess()->BindServiceInterface(std::move(receiver));
}

}  // namespace

// static
std::vector<ServiceProcessInfo> ServiceProcessHost::GetRunningProcessInfo() {
  return GetServiceProcessTracker().GetProcesses();
}

// static
void ServiceProcessHost::AddObserver(Observer* observer) {
  GetServiceProcessTracker().AddObserver(observer);
}

// static
void ServiceProcessHost::RemoveObserver(Observer* observer) {
  GetServiceProcessTracker().RemoveObserver(observer);
}

// static
void ServiceProcessHost::Launch(mojo::GenericPendingReceiver receiver,
                                Options options) {
  DCHECK(receiver.interface_name().has_value());
  base::CreateSingleThreadTaskRunner({BrowserThread::IO})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&LaunchServiceProcessOnIOThread,
                                std::move(receiver), std::move(options)));
}

}  // namespace content
