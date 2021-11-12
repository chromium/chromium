// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

namespace mac {
class PermissionWizard;
}

class DaemonControllerDelegateMac : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateMac();

  DaemonControllerDelegateMac(const DaemonControllerDelegateMac&) = delete;
  DaemonControllerDelegateMac& operator=(const DaemonControllerDelegateMac&) =
      delete;

  ~DaemonControllerDelegateMac() override;

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

 private:
  std::unique_ptr<mac::PermissionWizard> permission_wizard_;

  // Task runner used to run blocking calls that would otherwise block the UI
  // thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;
  AutoThread io_thread_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
