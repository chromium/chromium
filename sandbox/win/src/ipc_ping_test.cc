// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions_win.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Tests that the IPC is working by issuing a special IPC that is not exposed
// in the public API.
SBOX_TEST_COMMAND(IPC_Ping) {
  if (args.size() != 1) {
    return SBOX_TEST_FAILED;
  }

  TargetServices* ts = SandboxFactory::GetTargetServices();
  if (!ts) {
    return SBOX_TEST_FAILED;
  }

  // Downcast because we have internal knowledge of the object returned.
  TargetServicesBase* ts_base = reinterpret_cast<TargetServicesBase*>(ts);

  int version = 0;
  if (!base::StringToInt(args[0], &version)) {
    return SBOX_TEST_FAILED;
  }

  if (!ts_base->TestIPCPing(version)) {
    return SBOX_TEST_FAILED;
  }

  ::Sleep(1);
  if (!ts_base->TestIPCPing(version)) {
    return SBOX_TEST_FAILED;
  }

  return SBOX_TEST_SUCCEEDED;
}

// The IPC ping test should work before and after the token drop.
TEST(IPCTest, IPCPingTestSimple) {
  IPC_PingTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(1));
}

TEST(IPCTest, IPCPingTestWithOutput) {
  IPC_PingTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(2));
  IPC_PingTestRunner runner2;
  runner2.SetTimeout(2000);
  runner2.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(2));
}

}  // namespace sandbox
