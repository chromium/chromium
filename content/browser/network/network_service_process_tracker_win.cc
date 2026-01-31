// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_process_tracker_win.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {

class NetworkServiceListener : public ServiceProcessHost::Observer {
 public:
  NetworkServiceListener();

  NetworkServiceListener(const NetworkServiceListener&) = delete;
  NetworkServiceListener& operator=(const NetworkServiceListener&) = delete;

  ~NetworkServiceListener() override;

  const base::Process& GetNetworkServiceProcess() const;

  void AddNetworkServiceAwaitingClosure(base::OnceClosure on_available);

 private:
  // ServiceProcessHost::Observer implementation:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override;
  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override;
  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override;

  base::Process network_process_;

  // This intentionally doesn't use `base::ClosureList`, as the validity of the
  // closure is not tied to the lifetime of another object, so its subscription
  // semantics are not helpful. This might change if the
  // WaitForNetworkServiceProcess() API gains another user.
  std::vector<base::OnceClosure> awaiting_closures_;

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

void NetworkServiceListener::AddNetworkServiceAwaitingClosure(
    base::OnceClosure on_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  awaiting_closures_.emplace_back(std::move(on_available));
}

void NetworkServiceListener::OnServiceProcessLaunched(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>())
    return;
  network_process_ = info.GetProcess().Duplicate();
  // Guard against re-entrancy by moving the closures into a local variable
  // before running them.
  auto awaiting_closures = std::exchange(awaiting_closures_, {});
  for (auto& closure : awaiting_closures) {
    std::move(closure).Run();
  }
}

void NetworkServiceListener::OnServiceProcessTerminatedNormally(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>())
    return;
  network_process_ = base::Process();
}

void NetworkServiceListener::OnServiceProcessCrashed(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<network::mojom::NetworkService>())
    return;
  network_process_ = base::Process();
}

}  // namespace

base::Process GetNetworkServiceProcess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsInProcessNetworkService())
    return base::Process::Current().Duplicate();
  return GetInstance().GetNetworkServiceProcess().Duplicate();
}

CONTENT_EXPORT void WaitForNetworkServiceProcess(
    base::OnceClosure on_available) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!IsInProcessNetworkService());
  GetInstance().AddNetworkServiceAwaitingClosure(std::move(on_available));
}

}  // namespace content
