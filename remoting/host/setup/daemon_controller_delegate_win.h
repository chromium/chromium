// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_

#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateWin : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateWin();

  DaemonControllerDelegateWin(const DaemonControllerDelegateWin&) = delete;
  DaemonControllerDelegateWin& operator=(const DaemonControllerDelegateWin&) =
      delete;

  ~DaemonControllerDelegateWin() override;

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
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
