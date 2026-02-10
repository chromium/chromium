// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_IPC_ARGS_H_
#define SANDBOX_WIN_SRC_IPC_ARGS_H_

#include <array>
#include <variant>

#include "base/compiler_specific.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/crosscall_server.h"

namespace sandbox {

// Class to hold an IPC server call's types and argument values.
class IPCArgs {
 public:
  IPCArgs();
  ~IPCArgs();

  // Initializes the arguments for an IPC call.
  bool Initialize(CrossCallParamsEx* params);

  const IPCParamTypes& types() const LIFETIME_BOUND { return types_; }
  // Get the IPC argument value by index.
  void* operator[](size_t index) LIFETIME_BOUND;

 private:
  using IPCArgVariant = std::variant<void*, std::wstring, CountedBuffer>;
  IPCParamTypes types_;
  std::array<IPCArgVariant, kMaxIpcParams> args_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_IPC_ARGS_H_
