// Copyright 2013 The Chromium Authors. All rights reserved.
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
  std::unique_ptr<base::DictionaryValue> GetConfig() override;
  void CheckPermission(bool it2me, DaemonController::BoolCallback) override;
  void SetConfigAndStart(std::unique_ptr<base::DictionaryValue> config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(std::unique_ptr<base::DictionaryValue> config,
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
