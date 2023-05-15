// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/free_deleter.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/security_util.h"
#include "build/build_config.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool TestOpenProcess(DWORD desired_access, DWORD expected_access) {
  base::win::ScopedHandle process(
      ::OpenProcess(desired_access, FALSE, ::GetCurrentProcessId()));
  if (!process.is_valid() ||
      ::GetProcessId(process.get()) != ::GetCurrentProcessId()) {
    return false;
  }
  return base::win::GetGrantedAccess(process.get()) == expected_access;
}

bool TestOpenThread(DWORD thread_id,
                    DWORD desired_access,
                    DWORD expected_access) {
  base::win::ScopedHandle thread(
      ::OpenThread(desired_access, FALSE, thread_id));
  if (!thread.is_valid() || ::GetThreadId(thread.get()) != thread_id) {
    return false;
  }
  return base::win::GetGrantedAccess(thread.get()) == expected_access;
}

}  // namespace

SBOX_TESTS_COMMAND int Process_OpenToken(int argc, wchar_t** argv) {
  HANDLE token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &token)) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    }
  } else {
    ::CloseHandle(token);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int Process_OpenProcess(int argc, wchar_t** argv) {
  if (!TestOpenProcess(PROCESS_ALL_ACCESS, PROCESS_ALL_ACCESS)) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (!TestOpenProcess(MAXIMUM_ALLOWED, PROCESS_ALL_ACCESS)) {
    return SBOX_TEST_SECOND_ERROR;
  }

  return SBOX_TEST_SUCCEEDED;
}

DWORD CALLBACK DummyThread(LPVOID) {
  return 0;
}

SBOX_TESTS_COMMAND int Process_OpenThread(int argc, wchar_t** argv) {
  DWORD thread_id = ::GetCurrentThreadId();
  if (!TestOpenThread(thread_id, THREAD_ALL_ACCESS, THREAD_ALL_ACCESS)) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (!TestOpenThread(thread_id, MAXIMUM_ALLOWED, THREAD_ALL_ACCESS)) {
    return SBOX_TEST_SECOND_ERROR;
  }
  base::win::ScopedHandle thread(::CreateThread(
      nullptr, 0, DummyThread, nullptr, CREATE_SUSPENDED, &thread_id));
  if (!thread.is_valid() || !::TerminateThread(thread.get(), 0)) {
    return SBOX_TEST_THIRD_ERROR;
  }
  if (!TestOpenThread(thread_id, THREAD_ALL_ACCESS, THREAD_ALL_ACCESS)) {
    return SBOX_TEST_FOURTH_ERROR;
  }
  if (!TestOpenThread(thread_id, MAXIMUM_ALLOWED, THREAD_ALL_ACCESS)) {
    return SBOX_TEST_FIFTH_ERROR;
  }

  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int Process_Crash(int argc, wchar_t** argv) {
  __debugbreak();
  return SBOX_TEST_FAILED;
}
// Generate a event name, used to test thread creation.
std::wstring GenerateEventName(DWORD pid) {
  wchar_t buff[30] = {0};
  int res = swprintf_s(buff, sizeof(buff) / sizeof(buff[0]),
                       L"ProcessPolicyTest_%08x", pid);
  if (-1 != res) {
    return std::wstring(buff);
  }
  return std::wstring();
}

// This is the function that is called when testing thread creation.
// It is expected to set an event that the caller is waiting on.
DWORD WINAPI TestThreadFunc(LPVOID lpdwThreadParam) {
  std::wstring event_name = GenerateEventName(
      static_cast<DWORD>(reinterpret_cast<uintptr_t>(lpdwThreadParam)));
  if (!event_name.length())
    return 1;
  HANDLE event = ::OpenEvent(EVENT_ALL_ACCESS | EVENT_MODIFY_STATE, false,
                             event_name.c_str());
  if (!event)
    return 1;
  if (!SetEvent(event))
    return 1;
  return 0;
}

SBOX_TESTS_COMMAND int Process_CreateThread(int argc, wchar_t** argv) {
  DWORD pid = ::GetCurrentProcessId();
  std::wstring event_name = GenerateEventName(pid);
  if (!event_name.length())
    return SBOX_TEST_FIRST_ERROR;
  HANDLE event = ::CreateEvent(nullptr, true, false, event_name.c_str());
  if (!event)
    return SBOX_TEST_SECOND_ERROR;

  DWORD thread_id = 0;
  HANDLE thread = nullptr;
  thread = ::CreateThread(nullptr, 0, &TestThreadFunc,
                          reinterpret_cast<LPVOID>(static_cast<uintptr_t>(pid)),
                          0, &thread_id);

  if (!thread)
    return SBOX_TEST_THIRD_ERROR;
  if (!thread_id)
    return SBOX_TEST_FOURTH_ERROR;
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
    return SBOX_TEST_FIFTH_ERROR;
  DWORD exit_code = 0;
  if (!GetExitCodeThread(thread, &exit_code))
    return SBOX_TEST_SIXTH_ERROR;
  if (exit_code)
    return SBOX_TEST_SEVENTH_ERROR;
  if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
    return SBOX_TEST_FAILED;
  return SBOX_TEST_SUCCEEDED;
}

// Tests that the broker correctly handles a process crashing within the job.
// Fails on Windows ARM64: https://crbug.com/905526
#if defined(ARCH_CPU_ARM64)
#define MAYBE_CreateProcessCrashy DISABLED_CreateProcessCrashy
#else
#define MAYBE_CreateProcessCrashy CreateProcessCrashy
#endif
TEST(ProcessPolicyTest, MAYBE_CreateProcessCrashy) {
  TestRunner runner;
  EXPECT_EQ(static_cast<int>(STATUS_BREAKPOINT),
            runner.RunTest(L"Process_Crash"));
}

TEST(ProcessPolicyTest, OpenToken) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_OpenToken"));
}

TEST(ProcessPolicyTest, OpenProcess) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_OpenProcess"));
}

TEST(ProcessPolicyTest, OpenThread) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_OpenThread"));
}

// This tests that the CreateThread works with CSRSS not locked down.
// In other words, that the interception passes through OK.
TEST(ProcessPolicyTest, TestCreateThreadWithCsrss) {
  TestRunner runner(JobLevel::kUnprotected, USER_INTERACTIVE, USER_INTERACTIVE);
  runner.SetDisableCsrss(false);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_CreateThread"));
}

// This tests that the CreateThread works with CSRSS locked down.
// In other words, that the interception correctly works.
TEST(ProcessPolicyTest, TestCreateThreadWithoutCsrss) {
  TestRunner runner(JobLevel::kUnprotected, USER_INTERACTIVE, USER_INTERACTIVE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_CreateThread"));
}

// This tests that our CreateThread interceptors works when called directly.
TEST(ProcessPolicyTest, TestCreateThreadOutsideSandbox) {
  DWORD pid = ::GetCurrentProcessId();
  std::wstring event_name = GenerateEventName(pid);
  ASSERT_STRNE(nullptr, event_name.c_str());
  HANDLE event = ::CreateEvent(nullptr, true, false, event_name.c_str());
  EXPECT_NE(static_cast<HANDLE>(nullptr), event);

  DWORD thread_id = 0;
  HANDLE thread = nullptr;
  thread = TargetCreateThread(
      ::CreateThread, nullptr, 0, &TestThreadFunc,
      reinterpret_cast<LPVOID>(static_cast<uintptr_t>(pid)), 0, &thread_id);
  EXPECT_NE(static_cast<HANDLE>(nullptr), thread);
  EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(thread, INFINITE));
  EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(event, INFINITE));
}

}  // namespace sandbox
