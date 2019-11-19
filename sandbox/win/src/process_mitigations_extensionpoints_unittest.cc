// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <windows.h>

#include <psapi.h>

#include "base/scoped_native_library.h"
#include "base/win/registry.h"
#include "base/win/startup_information.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/hooking_dll.h"
#include "sandbox/win/tests/integration_tests/hooking_win_proc.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//------------------------------------------------------------------------------
// Internal Defines & Functions
//------------------------------------------------------------------------------

// hooking_dll defines
using WasHookCalledFunction = decltype(&hooking_dll::WasHookCalled);
using SetHookFunction = decltype(&hooking_dll::SetHook);
constexpr char g_hook_handler_func[] = "HookProc";
constexpr char g_was_hook_called_func[] = "WasHookCalled";
constexpr char g_set_hook_func[] = "SetHook";

// System mutex to prevent conflicting tests from running at the same time.
const wchar_t g_extension_point_test_mutex[] = L"ChromeExtensionTestMutex";

//------------------------------------------------------------------------------
// ExtensionPoint test helper function.
//
// Spawn Windows process (with or without mitigation enabled).
//------------------------------------------------------------------------------
bool SpawnWinProc(PROCESS_INFORMATION* pi, bool success_test, HANDLE* event) {
  base::win::StartupInformation startup_info;
  DWORD creation_flags = 0;

  if (!success_test) {
    DWORD64 flags =
        PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON;
    // This test only runs on >= Win8, so don't have to handle
    // illegal 64-bit flags on 32-bit <= Win7.
    size_t flags_size = sizeof(flags);

    if (!startup_info.InitializeProcThreadAttributeList(1) ||
        !startup_info.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &flags, flags_size)) {
      ADD_FAILURE();
      return false;
    }
    creation_flags = EXTENDED_STARTUPINFO_PRESENT;
  }

  // Command line must be writable.
  std::wstring cmd_writeable(hooking_win_proc::g_winproc_file);

  if (!::CreateProcessW(nullptr, &cmd_writeable[0], nullptr, nullptr, false,
                        creation_flags, nullptr, nullptr,
                        startup_info.startup_info(), pi)) {
    ADD_FAILURE();
    return false;
  }
  EXPECT_EQ(WAIT_OBJECT_0,
            ::WaitForSingleObject(*event, sandbox::SboxTestEventTimeout()));

  return true;
}

//------------------------------------------------------------------------------
// ExtensionPoint test helper function.
//
// 1. Spawn a Windows process (with or without mitigation enabled).
// 2. Load the hook Dll locally.
// 3. Create a global named event for the hook to trigger.
// 4. Start the hook (for the specific WinProc or globally).
// 5. Send a keystroke event.
// 6. Ask the hook Dll if it received a hook callback.
// 7. Cleanup the hooking.
// 8. Signal the Windows process to shutdown.
//
// Do NOT use any ASSERTs in this function.  Cleanup required.
//------------------------------------------------------------------------------
void TestWin8ExtensionPointHookWrapper(bool is_success_test, bool global_hook) {
  // Set up a couple global events that this test will use.
  HANDLE winproc_event =
      ::CreateEventW(nullptr, false, false, hooking_win_proc::g_winproc_event);
  if (!winproc_event) {
    ADD_FAILURE();
    return;
  }
  base::win::ScopedHandle scoped_winproc_event(winproc_event);

  HANDLE hook_event =
      ::CreateEventW(nullptr, false, false, hooking_dll::g_hook_event);
  if (!hook_event) {
    ADD_FAILURE();
    return;
  }
  base::win::ScopedHandle scoped_hook_event(hook_event);

  // 1. Spawn WinProc.
  PROCESS_INFORMATION proc_info = {};
  if (!SpawnWinProc(&proc_info, is_success_test, &winproc_event))
    return;

  // From this point on, no return on failure.  Cleanup required.
  bool all_good = true;

  // 2. Load the hook DLL.
  base::FilePath hook_dll_path(hooking_dll::g_hook_dll_file);
  base::ScopedNativeLibrary dll(hook_dll_path);
  EXPECT_TRUE(dll.is_valid());

  HOOKPROC hook_proc =
      reinterpret_cast<HOOKPROC>(dll.GetFunctionPointer(g_hook_handler_func));
  WasHookCalledFunction was_hook_called =
      reinterpret_cast<WasHookCalledFunction>(
          dll.GetFunctionPointer(g_was_hook_called_func));
  SetHookFunction set_hook = reinterpret_cast<SetHookFunction>(
      dll.GetFunctionPointer(g_set_hook_func));
  if (!hook_proc || !was_hook_called || !set_hook) {
    ADD_FAILURE();
    all_good = false;
  }

  // 3. Try installing the hook (either on a remote target thread,
  //    or globally).
  HHOOK hook = nullptr;
  if (all_good) {
    DWORD target = 0;
    if (!global_hook)
      target = proc_info.dwThreadId;
    hook = ::SetWindowsHookExW(WH_KEYBOARD, hook_proc, dll.get(), target);
    if (!hook) {
      ADD_FAILURE();
      all_good = false;
    } else
      // Pass the hook DLL the hook handle.
      set_hook(hook);
  }

  // 4. Inject a keyboard event.
  if (all_good) {
    // Note: that PostThreadMessage and SendMessage APIs will not deliver
    // a keystroke in such a way that triggers a "legitimate" hook.
    // Have to use targetless SendInput or keybd_event.  The latter is
    // less code and easier to work with.
    keybd_event(VkKeyScan(L'A'), 0, 0, 0);
    keybd_event(VkKeyScan(L'A'), 0, KEYEVENTF_KEYUP, 0);
    // Give it a chance to hit the hook handler...
    ::WaitForSingleObject(hook_event, sandbox::SboxTestEventTimeout());

    // 5. Did the hook get hit?  Was it expected to?
    if (global_hook)
      EXPECT_EQ((is_success_test ? true : false), was_hook_called());
    else
      // ***IMPORTANT: when targeting a specific thread id, the
      // PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE
      // mitigation does NOT disable the hook API.  It ONLY
      // stops global hooks from running in a process.  Hence,
      // the hook will hit (true) even in the "failure"
      // case for a non-global/targeted hook.
      EXPECT_EQ((is_success_test ? true : true), was_hook_called());
  }

  // 6. Disable hook.
  if (hook)
    EXPECT_TRUE(::UnhookWindowsHookEx(hook));

  // 7. Trigger shutdown of WinProc.
  if (proc_info.hProcess) {
    if (::PostThreadMessageW(proc_info.dwThreadId, WM_QUIT, 0, 0)) {
      // Note: The combination/perfect-storm of a Global Hook, in a
      // WinProc that has the EXTENSION_POINT_DISABLE mitigation ON, and the
      // use of the SendInput or keybd_event API to inject a keystroke,
      // results in the target becoming unresponsive.  If any one of these
      // states are changed, the problem does not occur.  This means the WM_QUIT
      // message is not handled and the call to WaitForSingleObject times out.
      // Therefore not checking the return val.
      ::WaitForSingleObject(winproc_event, sandbox::SboxTestEventTimeout());
    } else {
      // Ensure no strays.
      ::TerminateProcess(proc_info.hProcess, 0);
      ADD_FAILURE();
    }
    EXPECT_TRUE(::CloseHandle(proc_info.hThread));
    EXPECT_TRUE(::CloseHandle(proc_info.hProcess));
  }
}

//------------------------------------------------------------------------------
// ExtensionPoint test helper function.
//
// 1. Set up the AppInit Dll in registry settings. (Enable)
// 2. Spawn a Windows process (with or without mitigation enabled).
// 3. Check if the AppInit Dll got loaded in the Windows process or not.
// 4. Signal the Windows process to shutdown.
// 5. Restore original reg settings.
//
// Do NOT use any ASSERTs in this function.  Cleanup required.
//------------------------------------------------------------------------------
void TestWin8ExtensionPointAppInitWrapper(bool is_success_test) {
  // 0.5 Get path of current module.  The appropriate build of the
  //     AppInit DLL will be in the same directory (and the
  //     full path is needed for reg).
  wchar_t path[MAX_PATH];
  if (!::GetModuleFileNameW(nullptr, path, MAX_PATH)) {
    ADD_FAILURE();
    return;
  }
  // Only want the directory.  Switch file name for the AppInit DLL.
  base::FilePath full_dll_path(path);
  full_dll_path = full_dll_path.DirName();
  full_dll_path = full_dll_path.Append(hooking_dll::g_hook_dll_file);
  wchar_t* non_const = const_cast<wchar_t*>(full_dll_path.value().c_str());
  // Now make sure the path is in "short-name" form for registry.
  DWORD length = ::GetShortPathNameW(non_const, nullptr, 0);
  std::vector<wchar_t> short_name(length);
  if (!::GetShortPathNameW(non_const, &short_name[0], length)) {
    ADD_FAILURE();
    return;
  }

  // 1. Reg setup.
  const wchar_t* app_init_reg_path =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
  const wchar_t* dlls_value_name = L"AppInit_DLLs";
  const wchar_t* enabled_value_name = L"LoadAppInit_DLLs";
  const wchar_t* signing_value_name = L"RequireSignedAppInit_DLLs";
  std::wstring orig_dlls;
  std::wstring new_dlls;
  DWORD orig_enabled_value = 0;
  DWORD orig_signing_value = 0;
  base::win::RegKey app_init_key(HKEY_LOCAL_MACHINE, app_init_reg_path,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
  // Backup the existing settings.
  if (!app_init_key.Valid() || !app_init_key.HasValue(dlls_value_name) ||
      !app_init_key.HasValue(enabled_value_name) ||
      ERROR_SUCCESS != app_init_key.ReadValue(dlls_value_name, &orig_dlls) ||
      ERROR_SUCCESS !=
          app_init_key.ReadValueDW(enabled_value_name, &orig_enabled_value)) {
    ADD_FAILURE();
    return;
  }
  if (app_init_key.HasValue(signing_value_name)) {
    if (ERROR_SUCCESS !=
        app_init_key.ReadValueDW(signing_value_name, &orig_signing_value)) {
      ADD_FAILURE();
      return;
    }
  }

  // Set the new settings (obviously requires local admin privileges).
  new_dlls = orig_dlls;
  if (!orig_dlls.empty())
    new_dlls.append(L",");
  new_dlls.append(short_name.data());

  // From this point on, no return on failure.  Cleanup required.
  bool all_good = true;

  if (app_init_key.HasValue(signing_value_name)) {
    if (ERROR_SUCCESS !=
        app_init_key.WriteValue(signing_value_name, static_cast<DWORD>(0))) {
      ADD_FAILURE();
      all_good = false;
    }
  }
  if (ERROR_SUCCESS !=
          app_init_key.WriteValue(dlls_value_name, new_dlls.c_str()) ||
      ERROR_SUCCESS !=
          app_init_key.WriteValue(enabled_value_name, static_cast<DWORD>(1))) {
    ADD_FAILURE();
    all_good = false;
  }

  // 2. Spawn WinProc.
  HANDLE winproc_event = nullptr;
  base::win::ScopedHandle scoped_event;
  PROCESS_INFORMATION proc_info = {};
  if (all_good) {
    winproc_event = ::CreateEventW(nullptr, false, false,
                                   hooking_win_proc::g_winproc_event);
    if (!winproc_event) {
      ADD_FAILURE();
      all_good = false;
    } else {
      scoped_event.Set(winproc_event);
      if (!SpawnWinProc(&proc_info, is_success_test, &winproc_event))
        all_good = false;
    }
  }

  // 3. Check loaded modules in WinProc to see if the AppInit dll is loaded.
  bool dll_loaded = false;
  if (all_good) {
    std::vector<HMODULE>(modules);
    if (!base::win::GetLoadedModulesSnapshot(proc_info.hProcess, &modules)) {
      ADD_FAILURE();
      all_good = false;
    } else {
      for (HMODULE module : modules) {
        wchar_t name[MAX_PATH] = {};
        if (::GetModuleFileNameExW(proc_info.hProcess, module, name,
                                   MAX_PATH) &&
            ::wcsstr(name, hooking_dll::g_hook_dll_file)) {
          // Found it.
          dll_loaded = true;
          break;
        }
      }
    }
  }

  // Was the test result as expected?
  if (all_good)
    EXPECT_EQ((is_success_test ? true : false), dll_loaded);

  // 4. Trigger shutdown of WinProc.
  if (proc_info.hProcess) {
    if (::PostThreadMessageW(proc_info.dwThreadId, WM_QUIT, 0, 0)) {
      ::WaitForSingleObject(winproc_event, sandbox::SboxTestEventTimeout());
    } else {
      // Ensure no strays.
      ::TerminateProcess(proc_info.hProcess, 0);
      ADD_FAILURE();
    }
    EXPECT_TRUE(::CloseHandle(proc_info.hThread));
    EXPECT_TRUE(::CloseHandle(proc_info.hProcess));
  }

  // 5. Reg Restore
  EXPECT_EQ(ERROR_SUCCESS,
            app_init_key.WriteValue(enabled_value_name, orig_enabled_value));
  if (app_init_key.HasValue(signing_value_name))
    EXPECT_EQ(ERROR_SUCCESS,
              app_init_key.WriteValue(signing_value_name, orig_signing_value));
  EXPECT_EQ(ERROR_SUCCESS,
            app_init_key.WriteValue(dlls_value_name, orig_dlls.c_str()));
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported Extension Point Tests
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Disable extension points (MITIGATION_EXTENSION_POINT_DISABLE).
// >= Win8
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin8ExtensionPointPolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(sandbox::TESTPOLICY_EXTENSIONPOINT);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_EXTENSION_POINT_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(
      policy2->SetDelayedProcessMitigations(MITIGATION_EXTENSION_POINT_DISABLE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that a "legitimate" global hook CAN be set on the
// sandboxed proc/thread if the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation is not set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest,
     DISABLED_CheckWin8ExtensionPoint_GlobalHook_Success) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointHookWrapper(true /* is_success_test */,
                                    true /* global hook */);
}

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation prevents a global hook on WinProc.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest,
     DISABLED_CheckWin8ExtensionPoint_GlobalHook_Failure) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointHookWrapper(false /* is_success_test */,
                                    true /* global hook */);
}

// This test validates that a "legitimate" hook CAN be set on the sandboxed
// proc/thread if the MITIGATION_EXTENSION_POINT_DISABLE mitigation is not set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_Hook_Success) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointHookWrapper(true /* is_success_test */,
                                    false /* global hook */);
}

// *** Important: MITIGATION_EXTENSION_POINT_DISABLE does NOT prevent
// hooks targetted at a specific thread id.  It only prevents
// global hooks.  So this test does NOT actually expect the hook
// to fail (see TestWin8ExtensionPointHookWrapper function) even
// with the mitigation on.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_Hook_Failure) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointHookWrapper(false /* is_success_test */,
                                    false /* global hook */);
}

// This test validates that an AppInit Dll CAN be added to a target
// WinProc if the MITIGATION_EXTENSION_POINT_DISABLE mitigation is not set.
//
// MANUAL testing only.
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_AppInit_Success) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointAppInitWrapper(true /* is_success_test */);
}

// This test validates that setting the MITIGATION_EXTENSION_POINT_DISABLE
// mitigation prevents the loading of any AppInit Dll into WinProc.
//
// MANUAL testing only.
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, DISABLED_CheckWin8ExtensionPoint_AppInit_Failure) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  ScopedTestMutex mutex(g_extension_point_test_mutex);

  TestWin8ExtensionPointAppInitWrapper(false /* is_success_test */);
}

}  // namespace sandbox
