// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/scoped_libusb_device_handle.h"

#include "services/device/usb/usb_context.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

ScopedLibusbDeviceHandle::ScopedLibusbDeviceHandle(
    libusb_device_handle* handle,
    scoped_refptr<UsbContext> context)
    : handle_(handle), context_(std::move(context)) {}

ScopedLibusbDeviceHandle::ScopedLibusbDeviceHandle(
    ScopedLibusbDeviceHandle&& other)
    : handle_(other.handle_), context_(std::move(other.context_)) {
  other.handle_ = nullptr;
}

ScopedLibusbDeviceHandle::~ScopedLibusbDeviceHandle() {
  Reset();
}

void ScopedLibusbDeviceHandle::Reset() {
  libusb_close(handle_);
  handle_ = nullptr;
  context_.reset();
}

bool ScopedLibusbDeviceHandle::IsValid() const {
  return handle_ != nullptr;
}

}  // namespace device
