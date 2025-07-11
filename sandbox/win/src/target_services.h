// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_SERVICES_H_
#define SANDBOX_WIN_SRC_TARGET_SERVICES_H_

#include <optional>
#include "base/containers/span.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

class ProcessState {
 public:
  ProcessState();

  ProcessState(const ProcessState&) = delete;
  ProcessState& operator=(const ProcessState&) = delete;

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
};

// This class is an implementation of the  TargetServices.
// Look in the documentation of sandbox::TargetServices for more info.
// Do NOT add a destructor to this class without changing the implementation of
// the factory method.
class TargetServicesBase : public TargetServices {
 public:
  TargetServicesBase();

  TargetServicesBase(const TargetServicesBase&) = delete;
  TargetServicesBase& operator=(const TargetServicesBase&) = delete;

  // Public interface of TargetServices. See comments in sandbox.h.
  ResultCode Init() override;
  std::optional<base::span<const uint8_t>> GetDelegateData() override;
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
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_SERVICES_H_
