// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_

#include <memory>

#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateLinux : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateLinux();

  DaemonControllerDelegateLinux(const DaemonControllerDelegateLinux&) = delete;
  DaemonControllerDelegateLinux& operator=(
      const DaemonControllerDelegateLinux&) = delete;

  ~DaemonControllerDelegateLinux() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::optional<base::Value::Dict> GetConfig() override;
  void CheckPermission(bool it2me, DaemonController::BoolCallback) override;
  void SetConfigAndStart(base::Value::Dict config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(base::Value::Dict config,
                    DaemonController::CompletionCallback done) override;
  void Stop(DaemonController::CompletionCallback done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;

  // If |start_host| is true (the default), then SetConfigAndStart includes
  // the final step of starting the host (this step requires root, and hence
  // some interactive method of elevating). If |start_host| is false, then
  // SetConfigAndStart only sets the config, and it is up to the caller to
  // start the host if needed.
  static void set_start_host_after_setup(bool start_host);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
