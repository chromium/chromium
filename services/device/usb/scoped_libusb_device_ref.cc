// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/scoped_libusb_device_ref.h"

#include "services/device/usb/usb_context.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef() = default;

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef(libusb_device* device,
                                             scoped_refptr<UsbContext> context)
    : device_(device), context_(std::move(context)) {}

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef(
    const ScopedLibusbDeviceRef& other) {
  *this = other;
}

ScopedLibusbDeviceRef& ScopedLibusbDeviceRef::operator=(
    const ScopedLibusbDeviceRef& other) {
  if (this == &other) {
    return *this;
  }
  Reset();
  device_ = other.device_;
  context_ = other.context_;
  libusb_ref_device(device_);
  return *this;
}

ScopedLibusbDeviceRef::ScopedLibusbDeviceRef(ScopedLibusbDeviceRef&& other) {
  *this = std::move(other);
}

ScopedLibusbDeviceRef& ScopedLibusbDeviceRef::operator=(
    ScopedLibusbDeviceRef&& other) {
  if (this == &other) {
    return *this;
  }
  Reset();
  device_ = other.device_;
  context_ = std::move(other.context_);
  other.device_ = nullptr;
  return *this;
}

ScopedLibusbDeviceRef::~ScopedLibusbDeviceRef() {
  Reset();
}

void ScopedLibusbDeviceRef::Reset() {
  libusb_unref_device(device_.ExtractAsDangling());
}

bool ScopedLibusbDeviceRef::IsValid() const {
  return device_ != nullptr;
}

bool operator==(const ScopedLibusbDeviceRef& ref, libusb_device* device) {
  return ref.get() == device;
}

}  // namespace device
