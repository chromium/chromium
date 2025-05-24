// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_USB_DEVICE_MANAGER_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_USB_DEVICE_MANAGER_OVERRIDER_H_

#include "services/device/public/cpp/test/fake_usb_device_manager.h"

namespace device {

class ScopedUsbDeviceManagerOverrider {
 public:
  ScopedUsbDeviceManagerOverrider();
  ScopedUsbDeviceManagerOverrider(const ScopedUsbDeviceManagerOverrider&) =
      delete;
  ScopedUsbDeviceManagerOverrider& operator=(
      const ScopedUsbDeviceManagerOverrider&) = delete;
  ~ScopedUsbDeviceManagerOverrider();

  FakeUsbDeviceManager* device_manager() { return device_manager_.get(); }

 private:
  std::unique_ptr<FakeUsbDeviceManager> device_manager_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_USB_DEVICE_MANAGER_OVERRIDER_H_
