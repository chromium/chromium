// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_REF_H_
#define SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_REF_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

struct libusb_device;

namespace device {

class UsbContext;

// This class owns a reference to a libusb_device as well as a reference to
// the libusb_context. The libusb_context must outlive any libusb_device
// instances created from it.
class ScopedLibusbDeviceRef {
 public:
  ScopedLibusbDeviceRef();
  ScopedLibusbDeviceRef(libusb_device* device,
                        scoped_refptr<UsbContext> context);
  ScopedLibusbDeviceRef(ScopedLibusbDeviceRef&& other);
  ScopedLibusbDeviceRef& operator=(ScopedLibusbDeviceRef&& other);

  ScopedLibusbDeviceRef(const ScopedLibusbDeviceRef& other);
  ScopedLibusbDeviceRef& operator=(const ScopedLibusbDeviceRef& other);

  ~ScopedLibusbDeviceRef();

  libusb_device* get() const { return device_; }

  scoped_refptr<UsbContext> GetContext() const { return context_; }

  void Reset();
  bool IsValid() const;

 private:
  raw_ptr<libusb_device> device_;
  scoped_refptr<UsbContext> context_;
};

bool operator==(const ScopedLibusbDeviceRef& ref, libusb_device* device);

}  // namespace device

#endif  // SERVICES_DEVICE_USB_SCOPED_LIBUSB_DEVICE_REF_H_
