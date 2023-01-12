// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_stopper.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"

namespace remoting {

HostStopper::HostStopper(std::unique_ptr<ServiceClient> service_client,
                         scoped_refptr<DaemonController> daemon_controller)
    : service_client_(std::move(service_client)),
      daemon_controller_(daemon_controller) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

HostStopper::~HostStopper() = default;

void HostStopper::StopLocalHost(std::string access_token,
                                base::OnceClosure on_done) {
  DCHECK(!on_done_);
  access_token_ = access_token;
  on_done_ = std::move(on_done);
  daemon_controller_->GetConfig(
      base::BindOnce(&HostStopper::OnConfigLoaded, weak_ptr_));
}

void HostStopper::OnConfigLoaded(absl::optional<base::Value::Dict> config) {
  const std::string* hostId = nullptr;
  if (!config || !(hostId = config->FindString("host_id"))) {
    std::move(on_done_).Run();
    return;
  }

  LOG(INFO) << "Stopping existing host: " << *hostId
            << ". This may take a few seconds.";
  service_client_->UnregisterHost(*hostId, access_token_, this);
}

void HostStopper::StopHost() {
  daemon_controller_->Stop(base::BindOnce(&HostStopper::OnStopped, weak_ptr_));
}

void HostStopper::OnStopped(DaemonController::AsyncResult) {
  bool stopped = false;
  for (auto i = 0; !stopped && i < 10; i++) {
    stopped =
        (daemon_controller_->GetState() == DaemonController::STATE_STOPPED);
    if (!stopped) {
      base::PlatformThread::Sleep(base::Seconds(1));
    }
  }
  if (!stopped) {
    LOG(WARNING) << "Unable to stop existing host process. Setup will "
                 << "continue, but you may need to reboot to complete it.";
  }
  std::move(on_done_).Run();
}

void HostStopper::OnHostRegistered(const std::string& authorization_code) {
  NOTREACHED();
}

void HostStopper::OnHostUnregistered() {
  StopHost();
}

void HostStopper::OnOAuthError() {
  StopHost();
}

void HostStopper::OnNetworkError(int response_code) {
  StopHost();
}

}  // namespace remoting
