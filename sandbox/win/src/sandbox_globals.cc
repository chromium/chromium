// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

// The section for IPC and policy.
SANDBOX_INTERCEPT HANDLE g_shared_section = nullptr;

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt = {};

}  // namespace sandbox
