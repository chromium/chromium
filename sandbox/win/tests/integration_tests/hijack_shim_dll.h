// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HIJACK_SHIM_DLL_H_
#define SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HIJACK_SHIM_DLL_H_

// Hijack shim dll exported API defines for dynamic lookup.
constexpr char g_hijack_shim_func[] = "CheckHijackResult";

// Arg1: "true" or "false", if the DLL path should be in system32.
// Returns a sandbox::SboxTestResult value.
int CheckHijackResult(bool expect_system);

#endif  // SANDBOX_WIN_TESTS_INTEGRATION_TESTS_HIJACK_SHIM_DLL_H_
