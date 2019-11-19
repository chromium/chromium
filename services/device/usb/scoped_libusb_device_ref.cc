// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/scoped_libusb_device_ref.h"

#include "services/device/usb/usb_context.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef(libusb_device* device,
                                             scoped_refptr<UsbContext> context)
    : device_(device), context_(std::move(context)) {}

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef(ScopedLibusbDeviceRef&& other)
    : device_(other.device_), context_(std::move(other.context_)) {
  other.device_ = nullptr;
}

ScopedLibusbDeviceRef::~ScopedLibusbDeviceRef() {
  Reset();
}

void ScopedLibusbDeviceRef::Reset() {
  libusb_unref_device(device_);
  device_ = nullptr;
  context_.reset();
}

bool ScopedLibusbDeviceRef::IsValid() const {
  return device_ != nullptr;
}

bool operator==(const ScopedLibusbDeviceRef& ref, libusb_device* device) {
  return ref.get() == device;
}

}  // namespace device
