// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_ERROR_H_
#define SERVICES_DEVICE_USB_USB_ERROR_H_

#include <string>

namespace device {

// A number of libusb functions which return a member of enum libusb_error
// return an int instead so for convenience this function takes an int.
std::string ConvertPlatformUsbErrorToString(int errcode);

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_ERROR_H_
