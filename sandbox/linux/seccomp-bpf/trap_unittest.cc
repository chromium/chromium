// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/trap.h"

#include <signal.h>

#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

SANDBOX_TEST_ALLOW_NOISE(Trap, SigSysAction) {
  // This creates a global Trap instance, and registers the signal handler
  // (Trap::SigSysAction).
  Trap::Registry();

  // Send SIGSYS to self. If signal handler (SigSysAction) is not registered,
  // the process will be terminated with status code -SIGSYS.
  // Note that, SigSysAction handler would output an error message
  // "Unexpected SIGSYS received." so it is necessary to allow the noise.
  raise(SIGSYS);
}

}  // namespace
}  // namespace sandbox
