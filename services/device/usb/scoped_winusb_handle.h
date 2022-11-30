// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_SCOPED_WINUSB_HANDLE_H_
#define SERVICES_DEVICE_USB_SCOPED_WINUSB_HANDLE_H_

#include "base/win/scoped_handle.h"

extern "C" {
typedef void* WINUSB_INTERFACE_HANDLE;
}

namespace device {

class WinUsbHandleTraits {
 public:
  using Handle = WINUSB_INTERFACE_HANDLE;

  WinUsbHandleTraits() = delete;
  WinUsbHandleTraits(const WinUsbHandleTraits&) = delete;
  WinUsbHandleTraits& operator=(const WinUsbHandleTraits&) = delete;

  static bool CloseHandle(Handle handle);

  static bool IsHandleValid(Handle handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
  }

  static Handle NullHandle() { return nullptr; }
};

using ScopedWinUsbHandle =
    base::win::GenericScopedHandle<WinUsbHandleTraits,
                                   base::win::DummyVerifierTraits>;

}  // namespace device

#endif  // SERVICES_DEVICE_USB_SCOPED_WINUSB_HANDLE_H_
