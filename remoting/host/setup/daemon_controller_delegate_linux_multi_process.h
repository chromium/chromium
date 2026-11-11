// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class HostType;
class HostTypeEnvironment;

// DaemonController::Delegate implementation for the multi-process Linux host.
class DaemonControllerDelegateLinuxMultiProcess
    : public DaemonController::Delegate {
 public:
  // Creates a delegate for the multi-process `host_type`. SetConfigAndStart()
  // validates the context (see HostType::ValidateStartHostHookContext())
  // before writing the config, then runs the start hook of `host_type` (see
  // HostType::RunStartHostHook()). Stop() stops and disables the
  // multi-process host unit without changing the host type's state.
  // `local_user_name` is as described in
  // DaemonController::SetHostTypeForSetup(). `env` is used to query, start and
  // stop systemd units.
  DaemonControllerDelegateLinuxMultiProcess(
      const HostType& host_type,
      std::string_view local_user_name,
      std::unique_ptr<HostTypeEnvironment> env);

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

 private:
  raw_ref<const HostType> host_type_;
  std::string local_user_name_;
  std::unique_ptr<HostTypeEnvironment> env_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_MULTI_PROCESS_H_
