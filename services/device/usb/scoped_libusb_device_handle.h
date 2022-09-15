// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_
#define SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

struct libusb_device_handle;

namespace device {

class UsbContext;

// This class owns a reference to a libusb_device_handle as well as a reference
// to the libusb_context. The libusb_context must outlive any
// libusb_device_handle instances created from it.
class ScopedLibusbDeviceHandle {
 public:
  ScopedLibusbDeviceHandle(libusb_device_handle* handle,
                           scoped_refptr<UsbContext> context);
  ScopedLibusbDeviceHandle(ScopedLibusbDeviceHandle&& other);

  ScopedLibusbDeviceHandle(const ScopedLibusbDeviceHandle&) = delete;
  ScopedLibusbDeviceHandle& operator=(const ScopedLibusbDeviceHandle&) = delete;

  ~ScopedLibusbDeviceHandle();

  libusb_device_handle* get() const { return handle_; }

  void Reset();
  bool IsValid() const;

 private:
  raw_ptr<libusb_device_handle> handle_;
  scoped_refptr<UsbContext> context_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_HANDLE_H_
