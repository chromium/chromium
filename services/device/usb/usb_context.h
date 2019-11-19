// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_CONTEXT_H_
#define SERVICES_DEVICE_USB_USB_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

struct libusb_context;

namespace device {

typedef libusb_context* PlatformUsbContext;

// Ref-counted wrapper for libusb_context*.
// It also manages the life-cycle of UsbEventHandler.
// It is a blocking operation to delete UsbContext.
// Destructor must be called on FILE thread.
class UsbContext : public base::RefCountedThreadSafe<UsbContext> {
 public:
  explicit UsbContext(PlatformUsbContext context);

  PlatformUsbContext context() const { return context_; }

 protected:
  friend class base::RefCountedThreadSafe<UsbContext>;

  virtual ~UsbContext();

 private:
  class UsbEventHandler;

  PlatformUsbContext context_;
  std::unique_ptr<UsbEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(UsbContext);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_CONTEXT_H_
