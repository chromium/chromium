// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/scoped_libusb_device_handle.h"

#include "services/device/usb/usb_context.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

ScopedLibusbDeviceHandle::ScopedLibusbDeviceHandle(
    libusb_device_handle* handle,
    scoped_refptr<UsbContext> context,
    ScopedLibusbDeviceRef device)
    : handle_(handle),
      context_(std::move(context)),
      device_(std::move(device)) {}

ScopedLibusbDeviceHandle::ScopedLibusbDeviceHandle(
    ScopedLibusbDeviceHandle&& other) {
  *this = std::move(other);
}

ScopedLibusbDeviceHandle& ScopedLibusbDeviceHandle::operator=(
    ScopedLibusbDeviceHandle&& other) {
  if (this == &other) {
    return *this;
  }
  Reset();
  handle_ = other.handle_;
  context_ = std::move(other.context_);
  device_ = std::move(other.device_);
  other.handle_ = nullptr;
  return *this;
}

ScopedLibusbDeviceHandle::~ScopedLibusbDeviceHandle() {
  Reset();
}

void ScopedLibusbDeviceHandle::Reset() {
  libusb_close(handle_.ExtractAsDangling());
}

bool ScopedLibusbDeviceHandle::IsValid() const {
  return handle_ != nullptr;
}

}  // namespace device
