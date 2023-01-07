// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_error.h"

#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

std::string ConvertPlatformUsbErrorToString(int errcode) {
  return libusb_strerror(static_cast<libusb_error>(errcode));
}

}  // namespace device
