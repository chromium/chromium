// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_
#define SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class UsbDevice;
class UsbService;

class UsbTestGadget {
 public:
  enum Type {
    DEFAULT = 0,
    KEYBOARD,
    MOUSE,
    HID_ECHO,
    ECHO,
  };

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

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbTestGadget);
};

}  // namespace device

#endif  // SERVICES_DEVICE_TEST_USB_TEST_GADGET_H_
