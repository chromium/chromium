// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Tests that the IPC is working by issuing a special IPC that is not exposed
// in the public API.
SBOX_TESTS_COMMAND int IPC_Ping(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED;

  TargetServices* ts = SandboxFactory::GetTargetServices();
  if (!ts)
    return SBOX_TEST_FAILED;

  // Downcast because we have internal knowledge of the object returned.
  TargetServicesBase* ts_base = reinterpret_cast<TargetServicesBase*>(ts);

  int version = 0;
  if (L'1' == argv[0][0])
    version = 1;
  else
    version = 2;

  if (!ts_base->TestIPCPing(version))
    return SBOX_TEST_FAILED;

  ::Sleep(1);
  if (!ts_base->TestIPCPing(version))
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

// The IPC ping test should work before and after the token drop.
TEST(IPCTest, IPCPingTestSimple) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"IPC_Ping 1"));
}

TEST(IPCTest, IPCPingTestWithOutput) {
  TestRunner runner1;
  runner1.SetTimeout(2000);
  runner1.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner1.RunTest(L"IPC_Ping 2"));
  TestRunner runner2;
  runner2.SetTimeout(2000);
  runner2.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(L"IPC_Ping 2"));
}

}  // namespace sandbox
