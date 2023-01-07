// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_DLL_H_
#define SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_DLL_H_

#include <windows.h>

#ifdef BUILDING_DLL
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __declspec(dllimport)
#endif

namespace hooking_dll {

constexpr wchar_t g_hook_dll_file[] = L"sbox_integration_test_hooking_dll.dll";
constexpr wchar_t g_hook_event[] = L"ChromeExtensionTestHookEvent";

// System mutex to prevent conflicting tests from running at the same time.
// This particular mutex is related to the use of the hooking_dll.
constexpr wchar_t g_hooking_dll_mutex[] = L"ChromeTestHookingDllMutex";

DLL_EXPORT void SetHook(HHOOK hook_handle);
DLL_EXPORT bool WasHookCalled();
DLL_EXPORT LRESULT HookProc(int code, WPARAM w_param, LPARAM l_param);

}  // namespace hooking_dll

#endif  // SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_DLL_H_
