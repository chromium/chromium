// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/hooking_dll.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//------------------------------------------------------------------------------
// Internal Defines & Functions
//------------------------------------------------------------------------------

// Enum the dynamic code APIs being tested, to prevent hard coded int values.
enum DynCodeAPI {
  VIRTUALALLOC = 1,
  VIRTUALPROTECT,
  MAPVIEWCUSTOM,
  MAPVIEWFILE,
  NOTSUPPORTED  // Always leave this as the last enum.
};

// Advanced private function declaration.
void DynamicCodeTestHarness(sandbox::MitigationFlags which_mitigation,
                            bool expect_success,
                            bool enable_mitigation,
                            bool with_thread_opt_out = false);

// Common helper function for the different child process dynamic code tests.
//
// - VirtualAlloc with PAGE_EXECUTE_*
// - VirtualProtect with PAGE_EXECUTE_*
// - MapViewOfFile with FILE_MAP_EXECUTE | FILE_MAP_WRITE
int DynamicCodeTest(DynCodeAPI which_test, wchar_t* path) {
  switch (which_test) {
    case VIRTUALALLOC: {
      // Test VirtualAlloc with PAGE_EXECUTE_READWRITE.
      //-----------------------------------------------
      // Size rounds up to one page.
      void* allocation = ::VirtualAlloc(nullptr, 1, MEM_RESERVE | MEM_COMMIT,
                                        PAGE_EXECUTE_READWRITE);
      if (!allocation) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }
      ::VirtualFree(allocation, 0, MEM_RELEASE);
      break;
    }
    case VIRTUALPROTECT: {
      // Test VirtualProtect with PAGE_EXECUTE_READWRITE.
      //-------------------------------------------------
      // Use an existing executable function pointer.
      BYTE* function = reinterpret_cast<BYTE*>(&DynamicCodeTestHarness);
      DWORD old_protect, temp = 0;
      // Test making executable binary writable.
      if (!::VirtualProtect(function, sizeof(size_t), PAGE_EXECUTE_READWRITE,
                            &old_protect)) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }
      // Make sure to test the change back to executable.
      if (!::VirtualProtect(function, sizeof(size_t), old_protect, &temp)) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }
      break;
    }
    case MAPVIEWCUSTOM: {
      // Test MapViewOfFile with FILE_MAP_EXECUTE | FILE_MAP_WRITE.
      // (Custom created mapping.)
      //-----------------------------------------------------------
      HANDLE section =
          ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                               PAGE_EXECUTE_READWRITE, 0, 4096, L"TestMapping");
      if (!section) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }

      // Note: this test hinges on FILE_MAP_EXECUTE | FILE_MAP_WRITE access.
      // Any other access request will succeed even with the mitigation enabled.
      HANDLE* view = reinterpret_cast<HANDLE*>(::MapViewOfFile(
          section, FILE_MAP_EXECUTE | FILE_MAP_WRITE, 0, 0, 4096));

      if (!view) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }

      ::UnmapViewOfFile(view);
      ::CloseHandle(section);
      break;
    }
    case MAPVIEWFILE: {
      // Test MapViewOfFile with FILE_MAP_EXECUTE | FILE_MAP_WRITE.
      // (Existing file on disk mapping.)
      //-----------------------------------------------------------
      // Caller should have passed in a non-null file path.
      if (!path)
        return sandbox::SBOX_TEST_INVALID_PARAMETER;

      // Note: INVALID_HANDLE_VALUE
      HANDLE file_handle =
          ::CreateFile(path, GENERIC_EXECUTE | GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file_handle == INVALID_HANDLE_VALUE) {
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }

      HANDLE mapping_handle = ::CreateFileMapping(
          file_handle, nullptr, PAGE_EXECUTE_READWRITE, 0, 1, nullptr);
      if (!mapping_handle) {
        ::CloseHandle(file_handle);
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }

      // Note: this test hinges on FILE_MAP_EXECUTE | FILE_MAP_WRITE access.
      // Any other access request will succeed even with the mitigation enabled.
      void* view_start = ::MapViewOfFile(
          mapping_handle, FILE_MAP_EXECUTE | FILE_MAP_WRITE, 0, 0, 0);
      if (!view_start) {
        ::CloseHandle(mapping_handle);
        ::CloseHandle(file_handle);
        DWORD error = ::GetLastError();
        return static_cast<int>(error);
      }

      ::UnmapViewOfFile(view_start);
      ::CloseHandle(mapping_handle);
      ::CloseHandle(file_handle);
      break;
    }
    default:
      return sandbox::SBOX_TEST_INVALID_PARAMETER;
  }

  return sandbox::SBOX_TEST_SUCCEEDED;
}

// Thread class for testing dynamic code per-thread opt-out.
class DynamicCodeOptOutThread {
 public:
  // |path| optional, depending on |which_test|.
  DynamicCodeOptOutThread(bool mitigation,
                          DynCodeAPI which_test,
                          wchar_t* path = nullptr)
      : thread_(nullptr),
        opt_out_(mitigation),
        which_api_test_(which_test),
        file_path_(path),
        return_code_(sandbox::SBOX_TEST_NOT_FOUND) {}

  DynamicCodeOptOutThread(const DynamicCodeOptOutThread&) = delete;
  DynamicCodeOptOutThread& operator=(const DynamicCodeOptOutThread&) = delete;

  ~DynamicCodeOptOutThread() {
    if (thread_) {
      ::CloseHandle(thread_);
      thread_ = nullptr;
    }
  }

  // LPTHREAD_START_ROUTINE
  static DWORD WINAPI StaticThreadFunc(LPVOID lpParam) {
    DynamicCodeOptOutThread* this_thread =
        reinterpret_cast<DynamicCodeOptOutThread*>(lpParam);
    return this_thread->ThreadFunc();
  }

  // Main function.  Call this to create and start the test thread.
  // Call Join() to get the test result.
  void Start() {
    if (thread_)
      return;

    thread_ = ::CreateThread(nullptr, 0, StaticThreadFunc, this, 0, nullptr);
    if (!thread_) {
      return_code_ = ::GetLastError();
      return;
    }
  }

  // Wait for test thread to finish, and get the final test result.
  int Join() {
    // Handle case where thread creation failed.
    if (!thread_)
      return return_code_;

    // NOTE: TestTimeouts::action_max_timeout() is not long enough here.  In
    //       debug build this times out.
    DWORD timeout = ::IsDebuggerPresent() ? INFINITE : 5000;
    return_code_ = ::WaitForSingleObject(thread_, timeout);
    // Handle case of abnormal thread exit (or timeout).
    if (return_code_ != WAIT_OBJECT_0)
      return return_code_;

    if (!::GetExitCodeThread(thread_,
                             reinterpret_cast<DWORD*>(&return_code_))) {
      // Handle unexpected case of failing to get thread exit code.
      return_code_ = ::GetLastError();
      return return_code_;
    }

    return return_code_;
  }

 private:
  DWORD ThreadFunc() {
    // Opt-out this thread from disabled dynamic code.
    if (opt_out_) {
      if (!sandbox::ApplyMitigationsToCurrentThread(
              sandbox::MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD)) {
        return ::GetLastError();
      }
    }
    // Run the test.
    return DynamicCodeTest(which_api_test_, file_path_);
  }

  HANDLE thread_;
  bool opt_out_;
  DynCodeAPI which_api_test_;
  raw_ptr<wchar_t> file_path_;
  int return_code_;
};

// Helpers to set up rules for dynamic code tests, needed as policy
// (from the TestRunner) can only be applied to a single process.
std::unique_ptr<sandbox::TestRunner> RunnerWithMitigation(
    sandbox::MitigationFlags mitigations) {
  auto runner = std::make_unique<sandbox::TestRunner>();
  EXPECT_EQ(sandbox::SBOX_ALL_OK,
            runner->GetPolicy()->GetConfig()->SetDelayedProcessMitigations(
                mitigations));
  return runner;
}

//------------------------------------------------------------------------------
// DisableDynamicCode test harness helper function.  Tests numerous APIs.
// - APIs fail with ERROR_DYNAMIC_CODE_BLOCKED if this mitigation is
//   enabled and the target tries to meddle.
// - Acquire the global g_hooking_dll_mutex mutex before calling
//   (as we meddle with a shared system resource).
// - Note: Do not use ASSERTs in this function, as a global mutex is held.
//
// Trigger test child processes (with or without mitigation enabled).
//------------------------------------------------------------------------------
void DynamicCodeTestHarness(sandbox::MitigationFlags which_mitigation,
                            bool expect_success,
                            bool enable_mitigation,
                            bool with_thread_opt_out) {
  if (which_mitigation != sandbox::MITIGATION_DYNAMIC_CODE_DISABLE &&
      which_mitigation !=
          sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT) {
    ADD_FAILURE();
    return;
  }

  std::wstring shared =
      (which_mitigation == sandbox::MITIGATION_DYNAMIC_CODE_DISABLE)
          ? L"TestWin81DynamicCode "
          : L"TestWin10DynamicCodeWithOptOut ";
  if (which_mitigation ==
      sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT) {
    shared += (with_thread_opt_out) ? L"true" : L"false";
  }

  // Test 1:
  auto runner = enable_mitigation ? RunnerWithMitigation(which_mitigation)
                                  : std::make_unique<sandbox::TestRunner>();
  std::wstring test =
      base::StrCat({shared, L" ", base::NumberToWString(VIRTUALALLOC)});
  EXPECT_EQ((expect_success ? sandbox::SBOX_TEST_SUCCEEDED
                            : ERROR_DYNAMIC_CODE_BLOCKED),
            runner->RunTest(test.c_str()));

  // Test 2:
  runner = enable_mitigation ? RunnerWithMitigation(which_mitigation)
                             : std::make_unique<sandbox::TestRunner>();
  test = base::StrCat({shared, L" ", base::NumberToWString(VIRTUALPROTECT)});
  EXPECT_EQ((expect_success ? sandbox::SBOX_TEST_SUCCEEDED
                            : ERROR_DYNAMIC_CODE_BLOCKED),
            runner->RunTest(test.c_str()));

  // Test 3:
  // Need token level >= USER_LIMITED to be able to successfully run test 3.
  runner = enable_mitigation ? RunnerWithMitigation(which_mitigation)
                             : std::make_unique<sandbox::TestRunner>();
  EXPECT_EQ(sandbox::SBOX_ALL_OK,
            runner->GetPolicy()->GetConfig()->SetTokenLevel(
                sandbox::TokenLevel::USER_RESTRICTED_SAME_ACCESS,
                sandbox::TokenLevel::USER_LIMITED));

  test = base::StrCat({shared, L" ", base::NumberToWString(MAPVIEWCUSTOM)});
  EXPECT_EQ((expect_success ? sandbox::SBOX_TEST_SUCCEEDED
                            : ERROR_DYNAMIC_CODE_BLOCKED),
            runner->RunTest(test.c_str()));

  // Ensure sandbox access to the file on disk.
  base::FilePath dll_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll_path));
  dll_path = dll_path.Append(hooking_dll::g_hook_dll_file);

  // File must be writable, so create a writable copy in a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_dll_path =
      temp_dir.GetPath().Append(hooking_dll::g_hook_dll_file);
  ASSERT_TRUE(base::CopyFile(dll_path, temp_dll_path));

  runner = enable_mitigation ? RunnerWithMitigation(which_mitigation)
                             : std::make_unique<sandbox::TestRunner>();
  EXPECT_TRUE(runner->AllowFileAccess(sandbox::FileSemantics::kAllowAny,
                                      temp_dll_path.value().c_str()));

  test = base::StrCat({shared, L" ", base::NumberToWString(MAPVIEWFILE), L" \"",
                       temp_dll_path.value(), L"\""});
  EXPECT_EQ((expect_success ? sandbox::SBOX_TEST_SUCCEEDED
                            : ERROR_DYNAMIC_CODE_BLOCKED),
            runner->RunTest(test.c_str()));
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported functions called by child test processes.
//------------------------------------------------------------------------------

// Parse arguments and do the test.
//
// - Arg1 is a DynCodeAPI indicating which API to test.
// - [OPTIONAL] If Arg1 is MAPVIEWFILE, Arg2 is a file path to map.
SBOX_TESTS_COMMAND int TestWin81DynamicCode(int argc, wchar_t** argv) {
  if (argc < 1 || !argv[0])
    return SBOX_TEST_INVALID_PARAMETER;

  // Arg1
  int test = ::_wtoi(argv[0]);
  if (test <= 0 || test >= NOTSUPPORTED)
    return SBOX_TEST_INVALID_PARAMETER;

  // [OPTIONAL] Arg2
  wchar_t* path = nullptr;
  if (argc > 1)
    path = argv[1];

  return DynamicCodeTest(static_cast<DynCodeAPI>(test), path);
}

// Parse arguments and spawn the test thread.
//
// - Arg1 is a bool indicating whether to opt-out the test thread.
// - Arg2 is a DynCodeAPI indicating which API to test.
// - [OPTIONAL] If Arg2 is MAPVIEWFILE, Arg3 is a file path to map.
SBOX_TESTS_COMMAND int TestWin10DynamicCodeWithOptOut(int argc,
                                                      wchar_t** argv) {
  if (argc < 2 || !argv[0] || !argv[1])
    return SBOX_TEST_INVALID_PARAMETER;

  // Arg1
  bool opt_out = false;
  if (::wcsicmp(argv[0], L"true") == 0)
    opt_out = true;

  // Arg2
  int test = ::_wtoi(argv[1]);
  if (test <= 0 || test >= NOTSUPPORTED)
    return SBOX_TEST_INVALID_PARAMETER;

  // [OPTIONAL] Arg3
  wchar_t* path = nullptr;
  if (argc > 2)
    path = argv[2];

  // Spawn new thread and wait for it to finish!
  DynamicCodeOptOutThread opt_out_thread(opt_out, static_cast<DynCodeAPI>(test),
                                         path);
  opt_out_thread.Start();
  return opt_out_thread.Join();
}

//------------------------------------------------------------------------------
// Exported Dynamic Code Tests
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Disable dynamic code (MITIGATION_DYNAMIC_CODE_DISABLE)
// >= Win8.1
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_DYNAMIC_CODE_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin81DynamicCodePolicySuccess) {
// TODO(crbug.com/40559699): Windows ASan hotpatching requires dynamic code.
#if !defined(ADDRESS_SANITIZER)
  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_DYNAMICCODE);

//---------------------------------
// 1) Test setting pre-startup.
// **Currently only running pre-startup in release.  Due to the sandbox in the
// child using dynamic code for hooks, calls to "dynamic code APIs" are
// failing... silently in release, but assert/breakpoint in debug.  Since
// this test is only to check the policy setting, ignoring the failures is ok.
//---------------------------------
#if defined(NDEBUG)
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->GetConfig()->SetProcessMitigations(
                MITIGATION_DYNAMIC_CODE_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
#endif  // defined(NDEBUG)
  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(policy2->GetConfig()->SetDelayedProcessMitigations(
                MITIGATION_DYNAMIC_CODE_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
#endif
}

// This test validates that we can meddle with dynamic code if the
// MITIGATION_DYNAMIC_CODE_DISABLE mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin81DynamicCode_BaseCase) {
  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect success, no mitigation.
  DynamicCodeTestHarness(sandbox::MITIGATION_DYNAMIC_CODE_DISABLE,
                         true /* expect_success */,
                         false /* enable_mitigation */);
}

// This test validates that setting the MITIGATION_DYNAMIC_CODE_DISABLE
// mitigation prevents meddling with dynamic code.
TEST(ProcessMitigationsTest, CheckWin81DynamicCode_TestMitigation) {
  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect failure, with mitigation.
  DynamicCodeTestHarness(sandbox::MITIGATION_DYNAMIC_CODE_DISABLE,
                         false /* expect_success */,
                         true /* enable_mitigation */);
}

//------------------------------------------------------------------------------
// Disable dynamic code, with per-thread opt-out enabled
// (MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT).
// >= Win10_RS1 (Anniversary)
//------------------------------------------------------------------------------

// This test validates that setting the
// MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT mitigation enables the setting
// on a process.
TEST(ProcessMitigationsTest, CheckWin10DynamicCodeOptOutPolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

// TODO(crbug.com/40559699): Windows ASan hotpatching requires dynamic code.
#if !defined(ADDRESS_SANITIZER)
  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_DYNAMICCODEOPTOUT);

//---------------------------------
// 1) Test setting pre-startup.
// **Currently only running pre-startup in release.  Due to the sandbox in the
// child using dynamic code for hooks, calls to "dynamic code APIs" are
// failing... silently in release, but assert/breakpoint in debug.  Since
// this test is only to check the policy setting, ignoring the failures is ok.
//---------------------------------
#if defined(NDEBUG)
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->GetConfig()->SetProcessMitigations(
                MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
#endif  // defined(NDEBUG)
  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(policy2->GetConfig()->SetDelayedProcessMitigations(
                MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
#endif
}

// This test validates that we CAN meddle with dynamic code if the
// MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10DynamicCodeOptOut_BaseCase) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect success, no mitigation (and therefore no thread opt-out).
  DynamicCodeTestHarness(sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT,
                         true /* expect_success */,
                         false /* enable_mitigation */,
                         false /* with_thread_opt_out */);
}

// This test validates that setting the
// MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT mitigation BLOCKS meddling
// with dynamic code.
TEST(ProcessMitigationsTest, CheckWin10DynamicCodeOptOut_TestMitigation) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect failure, with mitigation, no thread opt-out.
  DynamicCodeTestHarness(sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT,
                         false /* expect_success */,
                         true /* enable_mitigation */,
                         false /* with_thread_opt_out */);
}

// This test validates that setting the
// MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT mitigation AND using
// thread-specific opt-out ALLOWS meddling with dynamic code.
TEST(ProcessMitigationsTest,
     CheckWin10DynamicCodeOptOut_TestMitigationWithOptOut) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect success, with mitigation, with thread opt-out.
  DynamicCodeTestHarness(sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT,
                         true /* expect_success */,
                         true /* enable_mitigation */,
                         true /* with_thread_opt_out */);
}

}  // namespace sandbox
