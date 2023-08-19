// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_PERMISSION_CHECKER_H_
#define REMOTING_HOST_MAC_PERMISSION_CHECKER_H_

#include "remoting/host/mac/permission_wizard.h"

#include "base/task/single_thread_task_runner.h"
#include "remoting/host/mac/permission_process_utils.h"

namespace remoting::mac {

class PermissionChecker : public PermissionWizard::Delegate {
 public:
  PermissionChecker(HostMode mode,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  PermissionChecker(const PermissionChecker&) = delete;
  PermissionChecker& operator=(const PermissionChecker&) = delete;
  ~PermissionChecker() override;

  std::string GetBundleName() override;
  void CheckAccessibilityPermission(
      PermissionWizard::ResultCallback onResult) override;
  void CheckScreenRecordingPermission(
      PermissionWizard::ResultCallback onResult) override;

 private:
  HostMode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace remoting::mac

#endif  // REMOTING_HOST_MAC_PERMISSION_CHECKER_H_
