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
#include "base/strings/string_util.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

#define BINDNTDLL(name)                                   \
  name##Function name = reinterpret_cast<name##Function>( \
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

// Reverts to self and verify that SetInformationToken was faked. Returns
// SBOX_TEST_SUCCEEDED if faked and SBOX_TEST_FAILED if not faked.
SBOX_TESTS_COMMAND int PolicyTargetTest_token(int argc, wchar_t** argv) {
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
SBOX_TESTS_COMMAND int PolicyTargetTest_steal(int argc, wchar_t** argv) {
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
    int ret = PolicyTargetTest_token(argc, argv);
    ::CloseHandle(thread_token);
    return ret;
  }
  return 0;
}

// Opens the thread token with and without impersonation.
SBOX_TESTS_COMMAND int PolicyTargetTest_token2(int argc, wchar_t** argv) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(),
                         TOKEN_IMPERSONATE | TOKEN_DUPLICATE, false,
                         &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  if (!OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                       true, &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Opens the thread token with and without impersonation, using
// NtOpenThreadTokenEX.
SBOX_TESTS_COMMAND int PolicyTargetTest_token3(int argc, wchar_t** argv) {
  BINDNTDLL(NtOpenThreadTokenEx);
  if (!NtOpenThreadTokenEx)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE thread_token;
  // Get the thread token, using impersonation.
  NTSTATUS status = NtOpenThreadTokenEx(GetCurrentThread(),
                                        TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                                        false, 0, &thread_token);
  if (status == STATUS_NO_TOKEN)
    return ERROR_NO_TOKEN;
  if (!NT_SUCCESS(status))
    return SBOX_TEST_FAILED;

  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  status = NtOpenThreadTokenEx(GetCurrentThread(),
                               TOKEN_IMPERSONATE | TOKEN_DUPLICATE, true, 0,
                               &thread_token);
  if (!NT_SUCCESS(status))
    return SBOX_TEST_FAILED;

  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can open the current thread.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread(int argc, wchar_t** argv) {
  DWORD thread_id = ::GetCurrentThreadId();
  HANDLE thread = ::OpenThread(SYNCHRONIZE, false, thread_id);
  if (!thread)
    return ::GetLastError();
  if (!::CloseHandle(thread))
    return ::GetLastError();

  return SBOX_TEST_SUCCEEDED;
}

// New thread entry point: do  nothing.
DWORD WINAPI PolicyTargetTest_thread_main(void* param) {
  ::Sleep(INFINITE);
  return 0;
}

// Tests that we can create a new thread, and open it.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread2(int argc, wchar_t** argv) {
  // Use default values to create a new thread.
  DWORD thread_id;
  HANDLE thread = ::CreateThread(nullptr, 0, &PolicyTargetTest_thread_main, 0,
                                 0, &thread_id);
  if (!thread)
    return ::GetLastError();
  if (!::CloseHandle(thread))
    return ::GetLastError();

  thread = ::OpenThread(SYNCHRONIZE, false, thread_id);
  if (!thread)
    return ::GetLastError();

  if (!::CloseHandle(thread))
    return ::GetLastError();

  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can call CreateProcess.
SBOX_TESTS_COMMAND int PolicyTargetTest_process(int argc, wchar_t** argv) {
  // Use default values to create a new process.
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION temp_process_info = {};
  // Note: CreateProcessW() can write to its lpCommandLine, don't pass a
  // raw string literal.
  std::wstring writable_cmdline_str(L"foo.exe");
  if (!::CreateProcessW(L"foo.exe", &writable_cmdline_str[0], nullptr, nullptr,
                        false, 0, nullptr, nullptr, &startup_info,
                        &temp_process_info))
    return SBOX_TEST_SUCCEEDED;
  base::win::ScopedProcessInformation process_info(temp_process_info);
  return SBOX_TEST_FAILED;
}

// Tests that environment is filtered correctly.
SBOX_TESTS_COMMAND int PolicyTargetTest_filterEnvironment(int argc,
                                                          wchar_t** argv) {
  auto env = base::Environment::Create();
  // "TMP" should never be filtered. See `TargetProcess::Create`.
  if (!env->HasVar("TMP")) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (env->HasVar("SBOX_TEST_ENV")) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(PolicyTargetTest, SetInformationThread) {
  TestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token"));

  TestRunner runner1;
  runner1.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner1.RunTest(L"PolicyTargetTest_token"));

  TestRunner runner2;
  runner2.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_FAILED, runner2.RunTest(L"PolicyTargetTest_steal"));
}

TEST(PolicyTargetTest, OpenThreadToken) {
  TestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token2"));

  TestRunner runner2;
  runner2.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner2.RunTest(L"PolicyTargetTest_token2"));
}

TEST(PolicyTargetTest, OpenThreadTokenEx) {
  TestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token3"));

  TestRunner runner2;
  runner2.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner2.RunTest(L"PolicyTargetTest_token3"));
}

TEST(PolicyTargetTest, OpenThread) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_thread"))
      << "Opens the current thread";

  TestRunner runner2;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(L"PolicyTargetTest_thread2"))
      << "Creates a new thread and opens it";
}

TEST(PolicyTargetTest, OpenProcess) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_process"))
      << "Opens a process";
}

// Sets the desktop for the current thread to be one with a null DACL, then
// launches a sandboxed app. Validates that the sandboxed app has access to the
// desktop.
TEST(PolicyTargetTest, InheritedDesktopPolicy) {
  // Create a desktop with a null dacl - which should allow access to
  // everything.
  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  SECURITY_DESCRIPTOR security_desc = {};
  ::InitializeSecurityDescriptor(&security_desc, SECURITY_DESCRIPTOR_REVISION);
  ::SetSecurityDescriptorDacl(&security_desc, true, nullptr, false);
  attributes.lpSecurityDescriptor = &security_desc;
  HDESK null_dacl_desktop_handle = CreateDesktop(
      L"null_dacl_desktop", nullptr, nullptr, 0, GENERIC_ALL, &attributes);
  EXPECT_TRUE(null_dacl_desktop_handle);

  // Switch to the null dacl desktop and run the test.
  HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  EXPECT_TRUE(null_dacl_desktop_handle);
  EXPECT_TRUE(::SetThreadDesktop(null_dacl_desktop_handle));

  BrokerServices* broker = GetBroker();

  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateDesktop));

  ASSERT_TRUE(broker);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  base::win::ScopedProcessInformation target;

  auto policy = broker->CreatePolicy();
  policy->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  PROCESS_INFORMATION temp_process_info = {};
  result = broker->SpawnTarget(prog_name, arguments.c_str(), std::move(policy),
                               &last_error, &temp_process_info);

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  // Run the process for some time to make sure it doesn't crash on launch
  EXPECT_EQ(1u, ::ResumeThread(target.thread_handle()));
  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));
  ::WaitForSingleObject(target.process_handle(), INFINITE);

  // Close the desktop handle.
  broker->DestroyDesktops();

  // Close the null dacl desktop.
  EXPECT_TRUE(::SetThreadDesktop(old_desktop));
  EXPECT_TRUE(::CloseDesktop(null_dacl_desktop_handle));
}

// Launches the app in the sandbox and ask it to wait in an
// infinite loop. Waits for 2 seconds and then check if the
// desktop associated with the app thread is not the same as the
// current desktop.
TEST(PolicyTargetTest, DesktopPolicy) {
  BrokerServices* broker = GetBroker();

  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateDesktop));

  ASSERT_TRUE(broker);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  base::win::ScopedProcessInformation target;

  auto policy = broker->CreatePolicy();
  policy->GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  PROCESS_INFORMATION temp_process_info = {};
  // Keep the desktop name to test against later (note - it was precreated).
  std::wstring desktop_name =
      broker->GetDesktopName(Desktop::kAlternateDesktop);
  result = broker->SpawnTarget(prog_name, arguments.c_str(), std::move(policy),
                               &last_error, &temp_process_info);

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  EXPECT_EQ(1u, ::ResumeThread(target.thread_handle()));

  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_NE(::GetThreadDesktop(target.thread_id()),
            ::GetThreadDesktop(::GetCurrentThreadId()));

  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, false, DESKTOP_ENUMERATE);
  EXPECT_TRUE(desk);
  EXPECT_TRUE(::CloseDesktop(desk));
  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));

  ::WaitForSingleObject(target.process_handle(), INFINITE);

  // Close the desktop handle.
  broker->DestroyDesktops();

  // Make sure the desktop does not exist anymore.
  desk = ::OpenDesktop(desktop_name.c_str(), 0, false, DESKTOP_ENUMERATE);
  EXPECT_FALSE(desk);
}

// Launches the app in the sandbox and ask it to wait in an
// infinite loop. Waits for 2 seconds and then check if the
// winstation associated with the app thread is not the same as the
// current desktop.
TEST(PolicyTargetTest, WinstaPolicy) {
  BrokerServices* broker = GetBroker();

  // Precreate the desktop.
  EXPECT_EQ(SBOX_ALL_OK,
            broker->CreateAlternateDesktop(Desktop::kAlternateWinstation));

  ASSERT_TRUE(broker);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  base::win::ScopedProcessInformation target;

  auto policy = broker->CreatePolicy();
  policy->GetConfig()->SetDesktop(Desktop::kAlternateWinstation);
  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  PROCESS_INFORMATION temp_process_info = {};
  DWORD last_error = ERROR_SUCCESS;
  // Keep the desktop name for later (note - it was precreated).
  std::wstring desktop_name =
      broker->GetDesktopName(Desktop::kAlternateWinstation);
  result = broker->SpawnTarget(prog_name, arguments.c_str(), std::move(policy),
                               &last_error, &temp_process_info);

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  EXPECT_EQ(1u, ::ResumeThread(target.thread_handle()));

  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_NE(::GetThreadDesktop(target.thread_id()),
            ::GetThreadDesktop(::GetCurrentThreadId()));

  ASSERT_FALSE(desktop_name.empty());

  // Make sure there is a backslash, for the window station name.
  EXPECT_NE(desktop_name.find_first_of(L'\\'), std::wstring::npos);

  // Isolate the desktop name.
  desktop_name = desktop_name.substr(desktop_name.find_first_of(L'\\') + 1);

  HDESK desk = ::OpenDesktop(desktop_name.c_str(), 0, false, DESKTOP_ENUMERATE);
  // This should fail if the desktop is really on another window station.
  EXPECT_FALSE(desk);
  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));

  ::WaitForSingleObject(target.process_handle(), INFINITE);

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
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);

  std::string_view contents = "Hello World";
  base::WritableSharedMemoryRegion writable_region =
      base::WritableSharedMemoryRegion::Create(contents.size());
  ASSERT_TRUE(writable_region.IsValid());
  base::WritableSharedMemoryMapping writable_mapping = writable_region.Map();
  ASSERT_TRUE(writable_mapping.IsValid());
  memcpy(writable_mapping.memory(), contents.data(), contents.size());

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  base::ReadOnlySharedMemoryRegion read_only_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(writable_region));
  ASSERT_TRUE(read_only_region.IsValid());

  auto policy = broker->CreatePolicy();
  policy->AddHandleToShare(read_only_region.GetPlatformHandle());

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 shared_memory_handle ";
  arguments += base::AsWString(base::NumberToString16(
      base::win::HandleToUint32(read_only_region.GetPlatformHandle())));

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  base::win::ScopedProcessInformation target;

  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  PROCESS_INFORMATION temp_process_info = {};
  DWORD last_error = ERROR_SUCCESS;
  result = broker->SpawnTarget(prog_name, arguments.c_str(), std::move(policy),
                               &last_error, &temp_process_info);

  EXPECT_EQ(SBOX_ALL_OK, result);
  if (result == SBOX_ALL_OK)
    target.Set(temp_process_info);

  EXPECT_EQ(1u, ::ResumeThread(target.thread_handle()));

  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::WaitForSingleObject(target.process_handle(), 2000));

  EXPECT_TRUE(::TerminateProcess(target.process_handle(), 0));

  ::WaitForSingleObject(target.process_handle(), INFINITE);
}

// Test if shared policies can be created by the broker.
TEST(SharedTargetConfig, BrokerConfigManagement) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);
  // Policies with empty names should not be fixed.
  auto policy = broker->CreatePolicy("");
  EXPECT_FALSE(policy->GetConfig()->IsConfigured());
  // Normally a policy is frozen (if necessary) by the broker when it is passed
  // to SpawnTarget.
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
    TestRunner runner;
    runner.GetPolicy()->GetConfig()->SetFilterEnvironment(/*filter=*/true);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED,
              runner.RunTest(L"PolicyTargetTest_filterEnvironment"));
  }
  {
    TestRunner runner;
    runner.GetPolicy()->GetConfig()->SetFilterEnvironment(/*filter=*/false);
    EXPECT_EQ(SBOX_TEST_SECOND_ERROR,
              runner.RunTest(L"PolicyTargetTest_filterEnvironment"));
  }
}

}  // namespace sandbox
