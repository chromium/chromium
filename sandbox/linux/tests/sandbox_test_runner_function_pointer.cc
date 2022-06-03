// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/sandbox_test_runner_function_pointer.h"

#include "base/check.h"
#include "build/build_config.h"

namespace sandbox {

SandboxTestRunnerFunctionPointer::SandboxTestRunnerFunctionPointer(
    void (*function_to_run)(void))
    : function_to_run_(function_to_run) {
}

SandboxTestRunnerFunctionPointer::~SandboxTestRunnerFunctionPointer() {
}

void SandboxTestRunnerFunctionPointer::Run() {
  DCHECK(function_to_run_);
  function_to_run_();
}

}  // namespace sandbox
