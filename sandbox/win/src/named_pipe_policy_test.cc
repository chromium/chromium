// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int NamedPipe_Create(int argc, wchar_t** argv) {
  if (argc < 1 || argc > 2 || !argv || !argv[0])
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE pipe = ::CreateNamedPipeW(
      argv[0], PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 4096, 4096, 2000, nullptr);
  if (INVALID_HANDLE_VALUE == pipe)
    return SBOX_TEST_DENIED;

  // The second parameter allows us to enforce an allowlist for where the
  // pipe should be in the object namespace after creation.
  if (argc == 2) {
    std::wstring handle_name;
    if (GetHandleName(pipe, &handle_name)) {
      if (handle_name.compare(0, wcslen(argv[1]), argv[1]) != 0)
        return SBOX_TEST_FAILED;
    } else {
      return SBOX_TEST_FAILED;
    }
  }

  OVERLAPPED overlapped = {0};
  overlapped.hEvent = ::CreateEvent(nullptr, true, true, nullptr);
  bool result = ::ConnectNamedPipe(pipe, &overlapped);

  if (!result) {
    DWORD error = ::GetLastError();
    if (ERROR_PIPE_CONNECTED != error && ERROR_IO_PENDING != error) {
      return SBOX_TEST_FAILED;
    }
  }

  if (!::CloseHandle(pipe))
    return SBOX_TEST_FAILED;

  ::CloseHandle(overlapped.hEvent);
  return SBOX_TEST_SUCCEEDED;
}

// Tests if we can create a pipe in the sandbox.
TEST(NamedPipePolicyTest, CreatePipe) {
  TestRunner runner;
  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\bleh"));
}

// Tests if we can create a pipe with a path traversal in the sandbox.
TEST(NamedPipePolicyTest, CreatePipeTraversal) {
  TestRunner runner;
  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\test\\..\\bleh"));
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\test/../bleh"));
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\test\\../bleh"));
  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\test/..\\bleh"));
}

// This tests that path canonicalization is actually disabled if we use \\?\
// syntax.
TEST(NamedPipePolicyTest, CreatePipeCanonicalization) {
  // "For file I/O, the "\\?\" prefix to a path string tells the Windows APIs to
  // disable all string parsing and to send the string that follows it straight
  // to the file system."
  // http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
  const wchar_t* argv[2] = {L"\\\\?\\pipe\\test\\..\\bleh",
                            L"\\Device\\NamedPipe\\test"};
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            NamedPipe_Create(2, const_cast<wchar_t**>(argv)));
}

// The same test as CreatePipe but this time using strict interceptions.
TEST(NamedPipePolicyTest, CreatePipeStrictInterceptions) {
  TestRunner runner;
  runner.GetPolicy()->SetStrictInterceptions();

  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\bleh"));
}

}  // namespace sandbox
