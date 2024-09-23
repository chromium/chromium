// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_
#define SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "services/device/usb/scoped_libusb_device_ref.h"

struct libusb_device_handle;

namespace device {

class UsbContext;

// This class owns a reference to a libusb_device_handle, libusb_device, and
// libusb_context. The libusb_context and libusb_device must outlive any
// libusb_device_handle instances created from it.
class ScopedLibusbDeviceHandle {
 public:
  ScopedLibusbDeviceHandle(libusb_device_handle* handle,
                           scoped_refptr<UsbContext> context,
                           ScopedLibusbDeviceRef device);
  ScopedLibusbDeviceHandle(ScopedLibusbDeviceHandle&& other);
  ScopedLibusbDeviceHandle& operator=(ScopedLibusbDeviceHandle&&);

  ScopedLibusbDeviceHandle(const ScopedLibusbDeviceHandle&) = delete;
  ScopedLibusbDeviceHandle& operator=(const ScopedLibusbDeviceHandle&) = delete;

  ~ScopedLibusbDeviceHandle();

  libusb_device_handle* get() const { return handle_; }

  bool IsValid() const;

 private:
  void Reset();

  raw_ptr<libusb_device_handle> handle_;
  scoped_refptr<UsbContext> context_;
  ScopedLibusbDeviceRef device_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_
