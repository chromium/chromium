// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_process_tracker_win.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {

// This value was chosen to be long enough that we can reasonably expect all
// mojo messages from the old network service to be have been processed, but not
// so long as to waste system resources long-term.
constexpr base::TimeDelta kKeepOldProcessHandlePeriod = base::Minutes(1);

// Permit the duration to be overridden at runtime for testing purposes.
constinit base::TimeDelta g_keep_old_process_handle_period =
    kKeepOldProcessHandlePeriod;

class NetworkServiceListener : public ServiceProcessHost::Observer {
 public:
  NetworkServiceListener();

  NetworkServiceListener(const NetworkServiceListener&) = delete;
  NetworkServiceListener& operator=(const NetworkServiceListener&) = delete;

  ~NetworkServiceListener() override;

  const base::Process& GetNetworkServiceProcess() const;

 private:
  // Most of the time the network service doesn't restart. Place the data needed
  // to keep the old PID valid in a separate object that is only allocated
  // when it is actually needed.
  class OldNetworkServiceProcess {
   public:
    // Constructs the object with `old_process`. `on_timeout` will be called
    // after `g_keep_old_process_handle_period` has elapsed.
    OldNetworkServiceProcess(base::Process old_process,
                             base::OnceClosure on_timeout);

    // Not copyable or assignable.
    OldNetworkServiceProcess(const OldNetworkServiceProcess&) = delete;
    OldNetworkServiceProcess& operator=(const OldNetworkServiceProcess&) =
        delete;

   private:
    base::Process old_process_;
    base::OneShotTimer timer_;
  };

  // If `network_process_` is valid, constructs an OldNetworkServiceProcess
  // object with it which starts the timer that will call
  // ResetOldNetworkServiceProcess()
  void SetNewNetworkProcess(base::Process new_network_process)
      VALID_CONTEXT_REQUIRED(owning_sequence_);

  // Frees `old_network_service_process_`.
  void ResetOldNetworkServiceProcess();

  // ServiceProcessHost::Observer implementation:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override;
  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override;
  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override;

  base::Process network_process_ GUARDED_BY_CONTEXT(owning_sequence_);

  // We keep a handle to at most one old network service process, until either
  // g_keep_old_process_handle_period has elapsed or another network service has
  // exited.
  std::unique_ptr<OldNetworkServiceProcess> old_network_service_process_
      GUARDED_BY_CONTEXT(owning_sequence_);

  SEQUENCE_CHECKER(owning_sequence_);
};

NetworkServiceListener& GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<NetworkServiceListener> listener;
  return *listener;
}

NetworkServiceListener::NetworkServiceListener() {
  ServiceProcessHost::AddObserver(this);
  auto running_processes = ServiceProcessHost::GetRunningProcessInfo();
  for (const auto& info : running_processes) {
    if (info.IsService<network::mojom::NetworkService>()) {
      network_process_ = info.GetProcess().Duplicate();
      break;
    }
  }
}

NetworkServiceListener::~NetworkServiceListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  ServiceProcessHost::RemoveObserver(this);
}

const base::Process& NetworkServiceListener::GetNetworkServiceProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return network_process_;
}

void NetworkServiceListener::SetNewNetworkProcess(
    base::Process new_network_process) {
  if (network_process_.IsValid()) {
    // This use of base::Unretained() is safe because
    // `old_network_service_process_` is owned by `this`.
    old_network_service_process_ = std::make_unique<OldNetworkServiceProcess>(
        std::move(network_process_),
        base::BindOnce(&NetworkServiceListener::ResetOldNetworkServiceProcess,
                       base::Unretained(this)));
  }
  network_process_ = std::move(new_network_process);
}

void NetworkServiceListener::ResetOldNetworkServiceProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  old_network_service_process_.reset();
}

void NetworkServiceListener::OnServiceProcessLaunched(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>()) {
    return;
  }
  SetNewNetworkProcess(info.GetProcess().Duplicate());
}

void NetworkServiceListener::OnServiceProcessTerminatedNormally(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>())
    return;
  SetNewNetworkProcess(base::Process());
}

void NetworkServiceListener::OnServiceProcessCrashed(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>())
    return;
  SetNewNetworkProcess(base::Process());
}

NetworkServiceListener::OldNetworkServiceProcess::OldNetworkServiceProcess(
    base::Process old_process,
    base::OnceClosure on_timeout)
    : old_process_(std::move(old_process)) {
  timer_.Start(FROM_HERE, g_keep_old_process_handle_period,
               std::move(on_timeout));
}

}  // namespace

void EnsureNetworkServiceListenerStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetInstance();
}

base::Process GetNetworkServiceProcessForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsInProcessNetworkService())
    return base::Process::Current().Duplicate();
  return GetInstance().GetNetworkServiceProcess().Duplicate();
}

ScopedKeepOldProcessHandlePeriodForTesting::
    ScopedKeepOldProcessHandlePeriodForTesting(base::TimeDelta duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(g_keep_old_process_handle_period, kKeepOldProcessHandlePeriod);
  g_keep_old_process_handle_period = duration;
}

ScopedKeepOldProcessHandlePeriodForTesting::
    ~ScopedKeepOldProcessHandlePeriodForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_keep_old_process_handle_period = kKeepOldProcessHandlePeriod;
}

}  // namespace content
