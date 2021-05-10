// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_
#define SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class sets up intercepts for the Win32K lockdown policy which is set
// on Windows 8 and beyond.
class ProcessMitigationsWin32KDispatcher : public Dispatcher {
 public:
  explicit ProcessMitigationsWin32KDispatcher(PolicyBase* policy_base);
  ~ProcessMitigationsWin32KDispatcher() override;

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) override;

 private:
  PolicyBase* policy_base_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMitigationsWin32KDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_MITIGATIONS_WIN32K_DISPATCHER_H_
