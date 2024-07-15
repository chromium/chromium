// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/browser/utility_process_host.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {

namespace {

// Changes to this function should be reviewed by a security person.
bool ShouldEnableSandbox(sandbox::mojom::Sandbox sandbox) {
  if (sandbox == sandbox::mojom::Sandbox::kAudio)
    return GetContentClient()->browser()->ShouldSandboxAudioService();
  if (sandbox == sandbox::mojom::Sandbox::kNetwork)
    return GetContentClient()->browser()->ShouldSandboxNetworkService();
  return true;
}

// Internal helper to track running service processes.
class ServiceProcessTracker {
 public:
  ServiceProcessTracker() = default;

  ServiceProcessTracker(const ServiceProcessTracker&) = delete;
  ServiceProcessTracker& operator=(const ServiceProcessTracker&) = delete;

  ~ServiceProcessTracker() = default;

  ServiceProcessInfo AddProcess(base::Process process,
                                const std::optional<GURL>& site,
                                const std::string& service_interface_name) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto id = GenerateNextId();
    ServiceProcessInfo info(service_interface_name, site, id,
                            std::move(process));
    auto info_dup = info.Duplicate();
    processes_.insert({id, std::move(info)});
    for (auto& observer : observers_)
      observer.OnServiceProcessLaunched(info_dup);
    return info_dup;
  }

  void NotifyTerminated(ServiceProcessId id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto iter = processes_.find(id);
    CHECK(iter != processes_.end(), base::NotFatalUntil::M130);

    for (auto& observer : observers_)
      observer.OnServiceProcessTerminatedNormally(iter->second.Duplicate());
    processes_.erase(iter);
  }

  void NotifyCrashed(ServiceProcessId id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto iter = processes_.find(id);
    CHECK(iter != processes_.end(), base::NotFatalUntil::M130);
    for (auto& observer : observers_)
      observer.OnServiceProcessCrashed(iter->second.Duplicate());
    processes_.erase(iter);
  }

  void AddObserver(ServiceProcessHost::Observer* observer) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ServiceProcessHost::Observer* observer) {
    // NOTE: Some tests may remove observers after BrowserThreads are shut down.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
           !BrowserThread::IsThreadInitialized(BrowserThread::UI));
    observers_.RemoveObserver(observer);
  }

  std::vector<ServiceProcessInfo> GetProcesses() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::vector<ServiceProcessInfo> processes;
    for (const auto& entry : processes_)
      processes.push_back(entry.second.Duplicate());
    return processes;
  }

 private:
  ServiceProcessId GenerateNextId() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return service_process_id_generator_.GenerateNextId();
  }

  ServiceProcessId::Generator service_process_id_generator_;

  std::map<ServiceProcessId, ServiceProcessInfo> processes_;

  // Observers are owned and used exclusively on the UI thread.
  base::ObserverList<ServiceProcessHost::Observer> observers_;
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
  UtilityProcessClient(
      const std::string& service_interface_name,
      const std::optional<GURL>& site,
      base::OnceCallback<void(const base::Process&)> process_callback)
      : service_interface_name_(service_interface_name),
        site_(std::move(site)),
        process_callback_(std::move(process_callback)) {}

  UtilityProcessClient(const UtilityProcessClient&) = delete;
  UtilityProcessClient& operator=(const UtilityProcessClient&) = delete;

  ~UtilityProcessClient() override = default;

  // UtilityProcessHost::Client:
  void OnProcessLaunched(const base::Process& process) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    process_info_.emplace(GetServiceProcessTracker().AddProcess(
        process.Duplicate(), site_, service_interface_name_));
    if (process_callback_) {
      std::move(process_callback_).Run(process);
    }
  }

  void OnProcessTerminatedNormally() override {
    GetServiceProcessTracker().NotifyTerminated(
        process_info_->service_process_id());
  }

  void OnProcessCrashed() override {
    // TODO(crbug.com/40654042): It is unclear how we can observe
    // |OnProcessCrashed()| without observing |OnProcessLaunched()| first, but
    // it can happen on Android. Ignore the notification in this case.
    if (!process_info_)
      return;

    GetServiceProcessTracker().NotifyCrashed(
        process_info_->service_process_id());
  }

 private:
  const std::string service_interface_name_;

  // Optional site GURL for per-site utility processes.
  const std::optional<GURL> site_;

  base::OnceCallback<void(const base::Process&)> process_callback_;
  std::optional<ServiceProcessInfo> process_info_;
};

// TODO(crbug.com/40633267): Once UtilityProcessHost is used only by service
// processes, its logic can be inlined here.
void LaunchServiceProcess(mojo::GenericPendingReceiver receiver,
                          ServiceProcessHost::Options options,
                          sandbox::mojom::Sandbox sandbox) {
  UtilityProcessHost* host =
      new UtilityProcessHost(std::make_unique<UtilityProcessClient>(
          *receiver.interface_name(), options.site,
          std::move(options.process_callback)));
  host->SetName(!options.display_name.empty()
                    ? options.display_name
                    : base::UTF8ToUTF16(*receiver.interface_name()));
  host->SetMetricsName(*receiver.interface_name());
  if (!ShouldEnableSandbox(sandbox)) {
    sandbox = sandbox::mojom::Sandbox::kNoSandbox;
  }
  host->SetSandboxType(sandbox);
  host->SetExtraCommandLineSwitches(std::move(options.extra_switches));
  if (options.child_flags) {
    host->set_child_flags(*options.child_flags);
  }
#if BUILDFLAG(IS_WIN)
  if (!options.preload_libraries.empty()) {
    host->SetPreloadLibraries(options.preload_libraries);
  }
#endif  // BUILDFLAG(IS_WIN)
  if (options.allow_gpu_client.has_value() &&
      options.allow_gpu_client.value()) {
    host->SetAllowGpuClient();
  }
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
                                Options options,
                                sandbox::mojom::Sandbox sandbox) {
  DCHECK(receiver.interface_name().has_value());
  if (GetUIThreadTaskRunner({})->BelongsToCurrentThread()) {
    LaunchServiceProcess(std::move(receiver), std::move(options), sandbox);
  } else {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&LaunchServiceProcess, std::move(receiver),
                                  std::move(options), sandbox));
  }
}

}  // namespace content
