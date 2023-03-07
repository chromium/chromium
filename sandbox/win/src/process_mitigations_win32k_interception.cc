// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_interception.h"

#include <windows.h>

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
