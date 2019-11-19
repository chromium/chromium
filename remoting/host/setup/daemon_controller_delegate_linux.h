// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_

#include <memory>

#include "base/macros.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateLinux : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateLinux();
  ~DaemonControllerDelegateLinux() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::unique_ptr<base::DictionaryValue> GetConfig() override;
  void CheckPermission(bool it2me, DaemonController::BoolCallback) override;
  void SetConfigAndStart(
      std::unique_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done) override;
  void UpdateConfig(std::unique_ptr<base::DictionaryValue> config,
                    const DaemonController::CompletionCallback& done) override;
  void Stop(const DaemonController::CompletionCallback& done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
