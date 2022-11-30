// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_WIN_PROC_H_
#define SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_WIN_PROC_H_

#include <windows.h>

namespace hooking_win_proc {

constexpr wchar_t g_winproc_file[] = L"sbox_integration_test_win_proc.exe ";
constexpr wchar_t g_winproc_event[] = L"ChromeExtensionTestWinProcEvent";

}  // namespace hooking_win_proc

#endif  // SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HOOKING_WIN_PROC_H_
