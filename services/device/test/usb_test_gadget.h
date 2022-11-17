// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_
#define SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class UsbDevice;
class UsbService;

// Declared here so that the deprecated URLFetcher class can friend it.
class UsbTestGadgetImpl;

class UsbTestGadget {
 public:
  enum Type {
    DEFAULT = 0,
    KEYBOARD,
    MOUSE,
    HID_ECHO,
    ECHO,
  };

  UsbTestGadget(const UsbTestGadget&) = delete;
  UsbTestGadget& operator=(const UsbTestGadget&) = delete;

  virtual ~UsbTestGadget() {}

  static bool IsTestEnabled();
  static std::unique_ptr<UsbTestGadget> Claim(
      UsbService* usb_service,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  virtual bool Unclaim() = 0;
  virtual bool Disconnect() = 0;
  virtual bool Reconnect() = 0;
  virtual bool SetType(Type type) = 0;

  virtual UsbDevice* GetDevice() const = 0;

 protected:
  UsbTestGadget() {}
};

}  // namespace device

#endif  // SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_
