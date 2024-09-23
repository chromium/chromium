// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <limits.h>
#include <stddef.h>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/handle_closer_agent.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kDeviceKsecDD[] = L"\\Device\\KsecDD";

// Used by the thread pool tests.
HANDLE finish_event;
const int kWaitCount = 20;

// Opens a handle to KsecDD in the same way as cryptbase.dll.
HANDLE OpenKsecDD() {
  UNICODE_STRING name;
  OBJECT_ATTRIBUTES attrs{};
  IO_STATUS_BLOCK iosb;
  HANDLE hDevice = INVALID_HANDLE_VALUE;

  RtlInitUnicodeString(&name, kDeviceKsecDD);
  InitializeObjectAttributes(&attrs, &name, 0, nullptr, nullptr);

  NTSTATUS status =
      NtOpenFile(&hDevice, 0x100001, &attrs, &iosb,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 FILE_SYNCHRONOUS_IO_NONALERT);

  if (!NT_SUCCESS(status)) {
    ::SetLastError(sandbox::GetLastErrorFromNtStatus(status));
    return INVALID_HANDLE_VALUE;
  }

  return hDevice;
}

}  // namespace

namespace sandbox {

// Checks for the presence of a list of files (in object path form).
// Format: CheckForFileHandle (Y|N) [devices or files to check for].
// - Y or N depending if the file should exist or not.
SBOX_TESTS_COMMAND int CheckForFileHandles(int argc, wchar_t** argv) {
  if (argc < 2) {
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }
  bool should_find = argv[0][0] == L'Y';
  if (argv[0][1] != L'\0' || (!should_find && argv[0][0] != L'N')) {
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }

  static int state = BEFORE_INIT;
  switch (state++) {
    case BEFORE_INIT: {
      HANDLE handle = OpenKsecDD();
      CHECK_NE(handle, INVALID_HANDLE_VALUE);
      // Leaks `handle` so we can check for it later.
      return SBOX_TEST_SUCCEEDED;
    }
    case AFTER_REVERT: {
      // Brute force the handle table to find what we're looking for.
      // This sort of matches what the closer does but it's a test of a sort.
      DWORD handle_count = UINT_MAX;
      const int kInvalidHandleThreshold = 100;
      const size_t kHandleOffset = 4;  // Handles are always a multiple of 4.
      HANDLE handle = nullptr;
      int invalid_count = 0;

      if (!::GetProcessHandleCount(::GetCurrentProcess(), &handle_count)) {
        return SBOX_TEST_FAILED_TO_RUN_TEST;
      }

      while (handle_count && invalid_count < kInvalidHandleThreshold) {
        reinterpret_cast<size_t&>(handle) += kHandleOffset;
        auto handle_name = GetPathFromHandle(handle);
        if (handle_name) {
          for (int i = 1; i < argc; ++i) {
            if (handle_name.value() == argv[i]) {
              return should_find ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
            }
          }
          --handle_count;
        } else {
          ++invalid_count;
        }
      }
      return should_find ? SBOX_TEST_FAILED : SBOX_TEST_SUCCEEDED;
    }
    default:
      break;  // Do nothing.
  }
  return SBOX_TEST_SUCCEEDED;
}

// Checks that closed handle becomes an Event and it's not waitable.
// Format: CheckForEventHandles
SBOX_TESTS_COMMAND int CheckForEventHandles(int argc, wchar_t** argv) {
  static int state = BEFORE_INIT;
  static HANDLE to_check;

  switch (state++) {
    case BEFORE_INIT: {
      // Ensure the process has a KsecDD handle.
      to_check = OpenKsecDD();
      CHECK_NE(to_check, INVALID_HANDLE_VALUE);
      return SBOX_TEST_SUCCEEDED;
    }
    case AFTER_REVERT: {
      auto type_name = GetTypeNameFromHandle(to_check);
      CHECK(type_name);
      CHECK(base::EqualsCaseInsensitiveASCII(type_name.value(), L"Event"));

      // Should not be able to wait.
      CHECK_EQ(WaitForSingleObject(to_check, INFINITE), WAIT_FAILED);

      // Should be able to close.
      CHECK(::CloseHandle(to_check));
      return SBOX_TEST_SUCCEEDED;
    }
    default:  // Do nothing.
      break;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleCloserTest, CheckForDeviceHandles) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);

  std::wstring command = std::wstring(L"CheckForFileHandles Y");
  command += (L" ");
  command += kDeviceKsecDD;

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str()))
      << "Failed: " << command;
}

TEST(HandleCloserTest, CloseSupportedDevices) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  std::wstring command = std::wstring(L"CheckForFileHandles N");
  command += (L" ");
  command += kDeviceKsecDD;

  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kDeviceApi);
  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kKsecDD);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str()))
      << "Failed: " << command;
}

TEST(HandleCloserTest, CheckStuffedHandle) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kDeviceApi);
  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kKsecDD);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckForEventHandles"));
}

void WINAPI ThreadPoolTask(void* event, BOOLEAN timeout) {
  static volatile LONG waiters_remaining = kWaitCount;
  CHECK(!timeout);
  CHECK(::CloseHandle(event));
  if (::InterlockedDecrement(&waiters_remaining) == 0)
    CHECK(::SetEvent(finish_event));
}

// Run a thread pool inside a sandbox without a CSRSS connection.
SBOX_TESTS_COMMAND int RunThreadPool(int argc, wchar_t** argv) {
  HANDLE wait_list[20];
  finish_event = ::CreateEvent(nullptr, true, false, nullptr);
  CHECK(finish_event);

  // Set up a bunch of waiters.
  HANDLE pool = nullptr;
  for (int i = 0; i < kWaitCount; ++i) {
    HANDLE event = ::CreateEvent(nullptr, true, false, nullptr);
    CHECK(event);
    CHECK(::RegisterWaitForSingleObject(&pool, event, ThreadPoolTask, event,
                                        INFINITE, WT_EXECUTEONLYONCE));
    wait_list[i] = event;
  }

  // Signal all the waiters.
  for (int i = 0; i < kWaitCount; ++i)
    CHECK(::SetEvent(wait_list[i]));

  CHECK_EQ(::WaitForSingleObject(finish_event, INFINITE), WAIT_OBJECT_0);
  CHECK(::CloseHandle(finish_event));

  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleCloserTest, RunThreadPool) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);

  // Sandbox policy will determine which platforms to disconnect CSRSS and when
  // to close the CSRSS handle.

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"RunThreadPool"));
}

}  // namespace sandbox
