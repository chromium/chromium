// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_HOST_STOPPER_H_
#define REMOTING_HOST_SETUP_HOST_STOPPER_H_

#include <optional>
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/service_client.h"

namespace remoting {

// A helper class that stops and unregisters a host.
class HostStopper final : public ServiceClient::Delegate {
 public:
  HostStopper(std::unique_ptr<ServiceClient> service_client,
              scoped_refptr<DaemonController> daemon_controller);
  HostStopper(const HostStopper&) = delete;
  HostStopper& operator=(const HostStopper&) = delete;
  ~HostStopper() override;

  // Stops the host running on the local computer, if any, and unregisters it.
  void StopLocalHost(std::string access_token, base::OnceClosure on_done);

 private:
  void OnConfigLoaded(std::optional<base::Value::Dict> config);
  void StopHost();
  void OnStopped(DaemonController::AsyncResult);

  // remoting::ServiceClient::Delegate
  void OnHostRegistered(const std::string& authorization_code) override;
  void OnHostUnregistered() override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  std::unique_ptr<remoting::ServiceClient> service_client_;
  scoped_refptr<remoting::DaemonController> daemon_controller_;
  std::string access_token_;
  base::OnceClosure on_done_;

  base::WeakPtr<HostStopper> weak_ptr_;
  base::WeakPtrFactory<HostStopper> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_HOST_STOPPER_H_
