// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <array>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
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
// Format: CheckForFileHandle should_find [devices or files to check for].
// - should_find is "1" or "0" depending if the file should exist or not.
SBOX_TEST_COMMAND(CheckForFileHandles) {
  if (args.size() < 2) {
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }

  bool should_find = args[0] == L"1";

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
          for (size_t i = 1; i < args.size(); ++i) {
            if (handle_name.value() == args[i]) {
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
SBOX_TEST_COMMAND(CheckForEventHandles) {
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
  CheckForFileHandlesTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(1, kDeviceKsecDD));
}

TEST(HandleCloserTest, CloseSupportedDevices) {
  CheckForFileHandlesTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kDeviceApi);
  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kKsecDD);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(0, kDeviceKsecDD));
}

TEST(HandleCloserTest, CheckStuffedHandle) {
  CheckForEventHandlesTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kDeviceApi);
  policy->GetConfig()->AddKernelObjectToClose(HandleToClose::kKsecDD);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

struct WaitTask {
  base::win::ScopedHandle event;
  HANDLE finish_event;
  raw_ptr<LONG> waiters_remaining;
  HANDLE wait_object;
};

void WINAPI ThreadPoolTask(void* param, BOOLEAN timeout) {
  CHECK(!timeout);
  WaitTask* task = static_cast<WaitTask*>(param);
  if (::InterlockedDecrement(task->waiters_remaining) == 0) {
    CHECK(::SetEvent(task->finish_event));
  }
}

// Run a thread pool inside a sandbox without a CSRSS connection.
SBOX_TEST_COMMAND(RunThreadPool) {
  constexpr int kWaitCount = 20;
  std::array<WaitTask, kWaitCount> wait_list;
  LONG waiters_remaining = kWaitCount;
  base::win::ScopedHandle finish_event(
      ::CreateEvent(nullptr, true, false, nullptr));
  CHECK(finish_event.is_valid());

  // Set up a bunch of waiters.
  for (WaitTask& task : wait_list) {
    task.event.Set(::CreateEvent(nullptr, true, false, nullptr));
    CHECK(task.event.is_valid());
    task.finish_event = finish_event.get();
    task.waiters_remaining = &waiters_remaining;
    CHECK(::RegisterWaitForSingleObject(&task.wait_object, task.event.get(),
                                        ThreadPoolTask, &task, INFINITE,
                                        WT_EXECUTEONLYONCE));
  }

  // Signal all the waiters.
  for (const WaitTask& task : wait_list) {
    CHECK(::SetEvent(task.event.get()));
  }

  CHECK_EQ(::WaitForSingleObject(finish_event.get(), INFINITE), WAIT_OBJECT_0);

  for (const WaitTask& task : wait_list) {
    CHECK(::UnregisterWait(task.wait_object));
  }

  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleCloserTest, RunThreadPool) {
  RunThreadPoolTestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);

  // Sandbox policy will determine which platforms to disconnect CSRSS and when
  // to close the CSRSS handle.

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

}  // namespace sandbox
