// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ntstatus.h>

#include <string_view>

#include "base/environment.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/windows_handle_util.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/window.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

SBOX_TEST_COMMAND(SharedMemoryCommand) {
  if (args.size() < 2) {
    return SBOX_TEST_FIRST_ERROR;
  }
  size_t raw_handle;
  if (!base::StringToSizeT(args[0], &raw_handle)) {
    return SBOX_TEST_SECOND_ERROR;
  }
  // First extract the handle to the platform-native ScopedHandle.
  base::win::ScopedHandle scoped_handle(reinterpret_cast<HANDLE>(raw_handle));
  if (!scoped_handle.is_valid()) {
    return SBOX_TEST_THIRD_ERROR;
  }

  auto test_contents = base::as_byte_span(args[1]);
  // Then convert to the low-level chromium region.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(scoped_handle),
          base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
          test_contents.size(), base::UnguessableToken::Create());
  // Finally wrap the low-level region in the shared memory API.
  base::ReadOnlySharedMemoryRegion region =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_region));
  if (!region.IsValid()) {
    return SBOX_TEST_FOURTH_ERROR;
  }
  base::ReadOnlySharedMemoryMapping view = region.Map();
  if (!view.IsValid()) {
    return SBOX_TEST_FIFTH_ERROR;
  }
  auto contents = base::span(view);
  if (contents != test_contents) {
    return SBOX_TEST_SIXTH_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

// Tests that environment is filtered correctly.
SBOX_TEST_COMMAND(FilterEnvironmentCommand) {
  auto env = base::Environment::Create();
  // "TMP" should never be filtered. See `CreateFilteredEnvironment`.
  if (!env->HasVar("TMP")) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (env->HasVar("SBOX_TEST_ENV")) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

}  // namespace

#define BINDNTDLL(name)                                   \
  name##Function name = reinterpret_cast<name##Function>( \
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

// Reverts to self and verify that SetInformationToken was faked. Returns
// SBOX_TEST_SUCCEEDED if faked and SBOX_TEST_FAILED if not faked.
SBOX_TEST_COMMAND(OpenTokenCommand) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(),
                         TOKEN_IMPERSONATE | TOKEN_DUPLICATE, false,
                         &thread_token))
    return ::GetLastError();

  ::RevertToSelf();
  ::CloseHandle(thread_token);

  int ret = SBOX_TEST_FAILED;
  if (::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                        false, &thread_token)) {
    ret = SBOX_TEST_SUCCEEDED;
    ::CloseHandle(thread_token);
  }
  return ret;
}

// Stores the high privilege token on a static variable, change impersonation
// again to that one and verify that we are not interfering anymore with
// RevertToSelf.
SBOX_TEST_COMMAND(StealTokenCommand) {
  static HANDLE thread_token;
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (!::OpenThreadToken(GetCurrentThread(),
                           TOKEN_IMPERSONATE | TOKEN_DUPLICATE, false,
                           &thread_token))
      return ::GetLastError();
  } else {
    if (!::SetThreadToken(nullptr, thread_token))
      return ::GetLastError();

    // See if we fake the call again.
    int ret = OpenTokenCommandImpl(args);
    ::CloseHandle(thread_token);
    return ret;
  }
  return 0;
}

// Opens the thread token with and without impersonation.
SBOX_TEST_COMMAND(OpenToken2Command) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(),
                         TOKEN_IMPERSONATE | TOKEN_DUPLICATE, false,
                         &thread_token)) {
    return ::GetLastError();
  }
  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  if (!OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                       true, &thread_token)) {
    return ::GetLastError();
  }
  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Opens the thread token with and without impersonation, using
// NtOpenThreadTokenEX.
SBOX_TEST_COMMAND(OpenToken3Command) {
  BINDNTDLL(NtOpenThreadTokenEx);
  if (!NtOpenThreadTokenEx) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  NTSTATUS status = NtOpenThreadTokenEx(GetCurrentThread(),
                                        TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                                        false, 0, &thread_token);
  if (status == STATUS_NO_TOKEN) {
    return ERROR_NO_TOKEN;
  }
  if (!NT_SUCCESS(status)) {
    return SBOX_TEST_FAILED;
  }
  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  status = NtOpenThreadTokenEx(GetCurrentThread(),
                               TOKEN_IMPERSONATE | TOKEN_DUPLICATE, true, 0,
                               &thread_token);
  if (!NT_SUCCESS(status)) {
    return SBOX_TEST_FAILED;
  }
  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can open the current thread.
SBOX_TEST_COMMAND(OpenThreadCommand) {
  DWORD thread_id = ::GetCurrentThreadId();
  base::win::ScopedHandle thread(::OpenThread(SYNCHRONIZE, false, thread_id));
  return thread.is_valid() ? SBOX_TEST_SUCCEEDED : ::GetLastError();
}

// New thread entry point: do  nothing.
DWORD WINAPI PolicyTargetTest_thread_main(void* param) {
  return 0;
}

// Tests that we can create a new thread, and open it.
SBOX_TEST_COMMAND(CreateThreadCommand) {
  // Use default values to create a new thread.
  DWORD thread_id;
  base::win::ScopedHandle thread(::CreateThread(
      nullptr, 0, &PolicyTargetTest_thread_main, 0, 0, &thread_id));
  if (!thread.is_valid()) {
    return ::GetLastError();
  }

  base::win::ScopedHandle thread2(::OpenThread(SYNCHRONIZE, false, thread_id));
  return thread2.is_valid() ? SBOX_TEST_SUCCEEDED : ::GetLastError();
}

// Tests that we can call CreateProcess.
SBOX_TEST_COMMAND(CreateProcessCommand) {
  // Use default values to create a new process.
  STARTUPINFO startup_info = {};
  PROCESS_INFORMATION proc_info = {};
  // Note: CreateProcessW() can write to its lpCommandLine, don't pass a
  // raw string literal.
  std::wstring cmd_line(L"foo.exe");
  if (!::CreateProcessW(L"foo.exe", std::data(cmd_line), nullptr, nullptr,
                        false, 0, nullptr, nullptr, &startup_info,
                        &proc_info)) {
    return SBOX_TEST_SUCCEEDED;
  }
  ::CloseHandle(proc_info.hProcess);
  ::CloseHandle(proc_info.hThread);
  return SBOX_TEST_FAILED;
}

SBOX_TEST_COMMAND(CheckDesktopNameCommand) {
  if (args.empty()) {
    return SBOX_TEST_FIRST_ERROR;
  }

  HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (!desktop) {
    return SBOX_TEST_SECOND_ERROR;
  }

  HWINSTA winsta =
      args[0].contains(L'\\') ? ::GetProcessWindowStation() : nullptr;

  if (GetFullDesktopName(winsta, desktop) != args[0]) {
    return SBOX_TEST_THIRD_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(PolicyTargetTest, SetInformationThread) {
  OpenTokenCommandTestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());

  OpenTokenCommandTestRunner runner1;
  runner1.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner1.RunTest());

  StealTokenCommandTestRunner runner2;
  runner2.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_FAILED, runner2.RunTest());
}

TEST(PolicyTargetTest, OpenThreadToken) {
  OpenToken2CommandTestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());

  OpenToken2CommandTestRunner runner2;
  runner2.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner2.RunTest());
}

TEST(PolicyTargetTest, OpenThreadTokenEx) {
  OpenToken3CommandTestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());

  OpenToken3CommandTestRunner runner2;
  runner2.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner2.RunTest());
}

TEST(PolicyTargetTest, OpenThread) {
  OpenThreadCommandTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest())
      << "Opens the current thread";

  CreateThreadCommandTestRunner runner2;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest())
      << "Creates a new thread and opens it";
}

TEST(PolicyTargetTest, OpenProcess) {
  CreateProcessCommandTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest()) << "Opens a process";
}

// Sets the desktop for the current thread to be one with a null DACL, then
// launches a sandboxed app. Validates that the sandboxed app has access to the
// desktop.
TEST(PolicyTargetTest, InheritedDesktopPolicy) {
  // Create a desktop with a null dacl - which should allow access to
  // everything.
  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  base::win::SecurityDescriptor sd;
  sd.set_dacl(*base::win::AccessControlList::FromPACL(nullptr));
  SECURITY_DESCRIPTOR security_desc = sd.ToAbsolute();
  attributes.lpSecurityDescriptor = &security_desc;
  HDESK null_dacl_desktop_handle = ::CreateDesktop(
      L"null_dacl_desktop", nullptr, nullptr, 0, GENERIC_ALL, &attributes);
  EXPECT_TRUE(null_dacl_desktop_handle);

  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);
  // Switch to the null dacl desktop and run the test.
  HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  EXPECT_TRUE(null_dacl_desktop_handle);
  EXPECT_TRUE(::SetThreadDesktop(null_dacl_desktop_handle));

  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateDesktop));
  // Close the null dacl desktop.
  EXPECT_TRUE(::SetThreadDesktop(old_desktop));
  EXPECT_TRUE(::CloseDesktop(null_dacl_desktop_handle));

  CheckDesktopNameCommandTestRunner runner;
  runner.SetTestState(BEFORE_INIT);
  runner.GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  std::wstring desktop_name =
      broker->GetDesktopName(Desktop::kAlternateDesktop);
  EXPECT_EQ(runner.RunTest(desktop_name), SBOX_TEST_SUCCEEDED);

  // Close the desktop handle.
  broker->DestroyDesktops();
}

// Launches the app in the sandbox and check it was assigned the alternative
// desktop on the current window station.
TEST(PolicyTargetTest, DesktopPolicy) {
  CheckDesktopNameCommandTestRunner runner;
  runner.SetTestState(BEFORE_INIT);
  BrokerServices* broker = runner.broker();

  ASSERT_TRUE(broker);
  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateDesktop));

  runner.GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  // Keep the desktop name to test against later (note - it was precreated).
  std::wstring desktop_name =
      broker->GetDesktopName(Desktop::kAlternateDesktop);
  EXPECT_EQ(runner.RunTest(desktop_name), SBOX_TEST_SUCCEEDED);
  // Close the desktop handle.
  broker->DestroyDesktops();
  // Make sure the desktop does not exist anymore.
  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, false, DESKTOP_ENUMERATE);
  EXPECT_FALSE(desk);
}

// Launches the app in the sandbox and check it was assigned the alternative
// desktop and window station.
TEST(PolicyTargetTest, WinstaPolicy) {
  CheckDesktopNameCommandTestRunner runner;
  runner.SetTestState(BEFORE_INIT);
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);

  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateWinstation));

  runner.GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  // Keep the desktop name for later (note - it was precreated).
  std::wstring desktop_name =
      broker->GetDesktopName(Desktop::kAlternateWinstation);

  // Make sure there is a backslash, for the window station name.
  ASSERT_TRUE(desktop_name.contains(L'\\'));
  EXPECT_EQ(runner.RunTest(desktop_name), SBOX_TEST_SUCCEEDED);

  // Isolate the desktop name.
  desktop_name = desktop_name.substr(desktop_name.find_first_of(L'\\') + 1);

  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, false, DESKTOP_ENUMERATE);
  // This should fail if the desktop is really on another window station.
  EXPECT_FALSE(desk);
  // Close the desktop handle.
  broker->DestroyDesktops();
}

// Creates multiple policies, with alternate desktops on both local and
// alternate winstations.
TEST(PolicyTargetTest, BothLocalAndAlternateWinstationDesktop) {
  BrokerServices* broker = GetBroker();

  auto policy1 = broker->CreatePolicy();
  auto policy2 = broker->CreatePolicy();
  auto policy3 = broker->CreatePolicy();

  ResultCode result;
  result = broker->CreateAlternateDesktop(Desktop::kAlternateDesktop);
  EXPECT_EQ(SBOX_ALL_OK, result);
  result = broker->CreateAlternateDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, result);

  policy1->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  policy2->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  policy3->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);

  std::wstring policy1_desktop_name =
      broker->GetDesktopName(Desktop::kAlternateDesktop);
  std::wstring policy2_desktop_name =
      broker->GetDesktopName(Desktop::kAlternateWinstation);

  // Extract only the "desktop name" portion of
  // "{winstation name}\\{desktop name}"
  EXPECT_NE(policy1_desktop_name.substr(
                policy1_desktop_name.find_first_of(L'\\') + 1),
            policy2_desktop_name.substr(
                policy2_desktop_name.find_first_of(L'\\') + 1));

  broker->DestroyDesktops();
}

// Launches the app in the sandbox and share a handle with it. The app should
// be able to use the handle.
TEST(PolicyTargetTest, ShareHandleTest) {
  std::wstring_view contents = L"Hello World";
  auto contents_span = base::as_byte_span(contents);
  base::WritableSharedMemoryRegion writable_region =
      base::WritableSharedMemoryRegion::Create(contents_span.size());
  ASSERT_TRUE(writable_region.IsValid());
  base::WritableSharedMemoryMapping writable_mapping = writable_region.Map();
  ASSERT_TRUE(writable_mapping.IsValid());
  std::ranges::copy(contents_span, base::span(writable_mapping).begin());

  base::ReadOnlySharedMemoryRegion read_only_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(writable_region));
  ASSERT_TRUE(read_only_region.IsValid());

  SharedMemoryCommandTestRunner runner(JobLevel::kLockdown, USER_INTERACTIVE,
                                       USER_LOCKDOWN);
  runner.SetTestState(BEFORE_INIT);
  runner.GetPolicy()->AddHandleToShare(read_only_region.GetPlatformHandle());
  EXPECT_EQ(runner.RunTest(
                base::win::HandleToUint32(read_only_region.GetPlatformHandle()),
                contents),
            SBOX_TEST_SUCCEEDED);
}

// Test if shared policies can be created by the broker.
TEST(SharedTargetConfig, BrokerConfigManagement) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);
  // Policies with empty names should not be fixed.
  auto policy = broker->CreatePolicy("");
  EXPECT_FALSE(policy->GetConfig()->IsConfigured());
  // Normally a policy is frozen (if necessary) by the broker when it is passed
  // to SpawnTargetAsync.
  BrokerServicesBase::FreezeTargetConfigForTesting(policy->GetConfig());
  EXPECT_TRUE(policy->GetConfig()->IsConfigured());
  auto policy_two = broker->CreatePolicy("");
  EXPECT_FALSE(policy_two->GetConfig()->IsConfigured());

  // Policies with no name should not be fixed.
  policy = broker->CreatePolicy();
  EXPECT_FALSE(policy->GetConfig()->IsConfigured());
  BrokerServicesBase::FreezeTargetConfigForTesting(policy->GetConfig());
  policy_two = broker->CreatePolicy();
  EXPECT_FALSE(policy_two->GetConfig()->IsConfigured());

  // Named policy should not be fixed the first time.
  policy = broker->CreatePolicy("key-one");
  EXPECT_FALSE(policy->GetConfig()->IsConfigured());
  BrokerServicesBase::FreezeTargetConfigForTesting(policy->GetConfig());
  // Policy should be fixed the second time.
  policy = broker->CreatePolicy("key-one");
  EXPECT_TRUE(policy->GetConfig()->IsConfigured());
  // Even if all policies with the same key are deleted.
  policy.reset();
  policy = broker->CreatePolicy("key-one");
  EXPECT_TRUE(policy->GetConfig()->IsConfigured());

  // A different name should not be fixed the first time.
  policy_two = broker->CreatePolicy("key-two");
  EXPECT_FALSE(policy_two->GetConfig()->IsConfigured());
  BrokerServicesBase::FreezeTargetConfigForTesting(policy_two->GetConfig());
  // But should be the second time.
  policy_two = broker->CreatePolicy("key-two");
  EXPECT_TRUE(policy_two->GetConfig()->IsConfigured());
}

// Test that environment for a sandboxed process is filtered correctly.
TEST(PolicyTargetTest, FilterEnvironment) {
  base::ScopedEnvironmentVariableOverride scoped_env("SBOX_TEST_ENV", "FOO");
  {
    FilterEnvironmentCommandTestRunner runner;
    runner.GetConfig()->SetFilterEnvironment(/*filter=*/true);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
  }
  {
    FilterEnvironmentCommandTestRunner runner;
    runner.GetConfig()->SetFilterEnvironment(/*filter=*/false);
    EXPECT_EQ(SBOX_TEST_SECOND_ERROR, runner.RunTest());
  }
}

TEST(PolicyTargetDeathTest, SharePseudoHandle) {
  SharedMemoryCommandTestRunner runner;
  EXPECT_DEATH(runner.GetPolicy()->AddHandleToShare(::GetCurrentThread()), "");
}

}  // namespace sandbox
