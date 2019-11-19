// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/scoped_winusb_handle.h"

#include <windows.h>
#include <winusb.h>

namespace device {

bool WinUsbHandleTraits::CloseHandle(Handle handle) {
  return WinUsb_Free(handle) == TRUE;
}

}  // namespace device
