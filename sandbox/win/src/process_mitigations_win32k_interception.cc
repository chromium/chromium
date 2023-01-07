// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

BOOL WINAPI
TargetGdiDllInitialize(GdiDllInitializeFunction orig_gdi_dll_initialize,
                       HANDLE dll,
                       DWORD reason) {
  return true;
}

HGDIOBJ WINAPI
TargetGetStockObject(GetStockObjectFunction orig_get_stock_object, int object) {
  return nullptr;
}

ATOM WINAPI
TargetRegisterClassW(RegisterClassWFunction orig_register_class_function,
                     const WNDCLASS* wnd_class) {
  return true;
}

}  // namespace sandbox
