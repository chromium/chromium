// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_

#include <memory>

#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

// DaemonController::Delegate implementation for the multi-process Linux host.
class DaemonControllerDelegateLinuxMultiProcess
    : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateLinuxMultiProcess();

  DaemonControllerDelegateLinuxMultiProcess(
      const DaemonControllerDelegateLinuxMultiProcess&) = delete;
  DaemonControllerDelegateLinuxMultiProcess& operator=(
      const DaemonControllerDelegateLinuxMultiProcess&) = delete;

  ~DaemonControllerDelegateLinuxMultiProcess() override;

  // Returns the config path that is the source of truth. It may contain
  // sensitive information, and may not be readable by the current user.
  static base::FilePath GetPrivilegedConfigPath();

  // Returns a config path that can be accessed by an unprivileged user. Note
  // that the returned config file could still contain sensitive information.
  static base::FilePath GetUnprivilegedConfigPath();

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::optional<base::DictValue> GetConfig() override;
  void CheckPermission(bool it2me,
                       DaemonController::BoolCallback callback) override;
  void SetConfigAndStart(base::DictValue config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(base::DictValue config,
                    DaemonController::CompletionCallback done) override;
  void Stop(DaemonController::CompletionCallback done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;
  bool is_privileged() const override;
  bool is_multi_process() const override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_
