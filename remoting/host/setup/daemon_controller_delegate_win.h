// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/values.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateWin : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateWin();
  explicit DaemonControllerDelegateWin(const base::FilePath& config_dir);

  DaemonControllerDelegateWin(const DaemonControllerDelegateWin&) = delete;
  DaemonControllerDelegateWin& operator=(const DaemonControllerDelegateWin&) =
      delete;

  ~DaemonControllerDelegateWin() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::optional<base::DictValue> GetConfig() override;
  void CheckPermission(bool it2me, DaemonController::BoolCallback) override;
  void SetConfigAndStart(base::DictValue config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(base::DictValue config,
                    DaemonController::CompletionCallback done) override;
  void Stop(DaemonController::CompletionCallback done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;
  bool is_privileged() const override;

  void set_config_dir_for_testing(const base::FilePath& config_dir) {
    config_dir_ = config_dir;
  }

 private:
  base::FilePath config_dir_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
