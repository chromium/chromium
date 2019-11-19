// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

namespace mac {
class PermissionWizard;
}

class DaemonControllerDelegateMac : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateMac();
  ~DaemonControllerDelegateMac() override;

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
  std::unique_ptr<mac::PermissionWizard> permission_wizard_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
