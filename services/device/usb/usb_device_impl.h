// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_IMPL_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_IMPL_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "services/device/usb/scoped_libusb_device_ref.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device.h"

struct libusb_device_descriptor;

namespace base {
class SequencedTaskRunner;
}

namespace device {

class ScopedLibusbDeviceHandle;
class UsbDeviceHandleImpl;

class UsbDeviceImpl : public UsbDevice {
 public:
  UsbDeviceImpl(ScopedLibusbDeviceRef platform_device,
                const libusb_device_descriptor& descriptor);

  // UsbDevice implementation:
  void Open(OpenCallback callback) override;

  // These functions are used during enumeration only. The values must not
  // change during the object's lifetime.
  void set_manufacturer_string(const base::string16& value) {
    device_info_->manufacturer_name = value;
  }
  void set_product_string(const base::string16& value) {
    device_info_->product_name = value;
  }
  void set_serial_number(const base::string16& value) {
    device_info_->serial_number = value;
  }
  void set_webusb_landing_page(const GURL& url) {
    device_info_->webusb_landing_page = url;
  }

  libusb_device* platform_device() const { return platform_device_.get(); }

 protected:
  friend class UsbServiceImpl;
  friend class UsbDeviceHandleImpl;

  ~UsbDeviceImpl() override;

  void ReadAllConfigurations();
  void RefreshActiveConfiguration();

  // Called only by UsbServiceImpl.
  void set_visited(bool visited) { visited_ = visited; }
  bool was_visited() const { return visited_; }

 private:
  void GetAllConfigurations();
  void OpenOnBlockingThread(
      OpenCallback callback,
      scoped_refptr<base::TaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  void Opened(ScopedLibusbDeviceHandle platform_handle,
              OpenCallback callback,
              scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  SEQUENCE_CHECKER(sequence_checker_);
  bool visited_ = false;

  const ScopedLibusbDeviceRef platform_device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_IMPL_H_
