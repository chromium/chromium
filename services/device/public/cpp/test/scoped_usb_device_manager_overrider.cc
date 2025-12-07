// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_usb_device_manager_overrider.h"

#include "base/functional/callback_helpers.h"
#include "services/device/device_service.h"

namespace device {

ScopedUsbDeviceManagerOverrider::ScopedUsbDeviceManagerOverrider() {
  device_manager_ = std::make_unique<FakeUsbDeviceManager>();
  DeviceService::OverrideUsbDeviceManagerBinderForTesting(
      base::BindRepeating(&FakeUsbDeviceManager::AddReceiver,
                          base::Unretained(device_manager_.get())));
}

ScopedUsbDeviceManagerOverrider::~ScopedUsbDeviceManagerOverrider() {
  DeviceService::OverrideUsbDeviceManagerBinderForTesting(base::NullCallback());
}

}  // namespace device
