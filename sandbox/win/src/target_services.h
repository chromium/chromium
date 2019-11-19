// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_TARGET_SERVICES_H__
#define SANDBOX_SRC_TARGET_SERVICES_H__

#include "base/macros.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

class ProcessState {
 public:
  ProcessState();
  // Returns true if main has been called.
  bool InitCalled() const;
  // Returns true if LowerToken has been called.
  bool RevertedToSelf() const;
  // Returns true if Csrss is connected.
  bool IsCsrssConnected() const;
  // Set the current state.
  void SetInitCalled();
  void SetRevertedToSelf();
  void SetCsrssConnected(bool csrss_connected);

 private:
  enum class ProcessStateInternal { NONE = 0, INIT_CALLED, REVERTED_TO_SELF };

  ProcessStateInternal process_state_;
  bool csrss_connected_;
  DISALLOW_COPY_AND_ASSIGN(ProcessState);
};

// This class is an implementation of the  TargetServices.
// Look in the documentation of sandbox::TargetServices for more info.
// Do NOT add a destructor to this class without changing the implementation of
// the factory method.
class TargetServicesBase : public TargetServices {
 public:
  TargetServicesBase();

  // Public interface of TargetServices.
  ResultCode Init() override;
  void LowerToken() override;
  ProcessState* GetState() override;

  // Factory method.
  static TargetServicesBase* GetInstance();

  // Sends a simple IPC Message that has a well-known answer. Returns true
  // if the IPC was successful and false otherwise. There are 2 versions of
  // this test: 1 and 2. The first one send a simple message while the
  // second one send a message with an in/out param.
  bool TestIPCPing(int version);

 private:
  ~TargetServicesBase() {}
  ProcessState process_state_;
  DISALLOW_COPY_AND_ASSIGN(TargetServicesBase);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_TARGET_SERVICES_H__
