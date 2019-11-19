// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "services/device/usb/usb_descriptors.h"
#include "url/gurl.h"

namespace device {

class UsbDeviceHandle;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. Methods other than simple property accessors must be
// called from the thread on which this object was created. For further
// manipulation of the device, a UsbDeviceHandle must be created from Open()
// method.
class UsbDevice : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  using OpenCallback = base::OnceCallback<void(scoped_refptr<UsbDeviceHandle>)>;
  using ResultCallback = base::OnceCallback<void(bool success)>;

  // This observer interface should be used by objects that need only be
  // notified about the removal of a particular device as it is more efficient
  // than registering a large number of observers with UsbService::AddObserver.
  class Observer {
   public:
    virtual ~Observer();

    // This method is called when the UsbService that created this object
    // detects that the device has been disconnected from the host.
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
  };

  const mojom::UsbDeviceInfo& device_info() const { return *device_info_; }

  // A unique identifier which remains stable for the lifetime of this device
  // object (i.e., until the device is unplugged or the USB service dies.)
  const std::string& guid() const { return device_info_->guid; }

  // Accessors to basic information.
  uint32_t bus_number() const { return device_info_->bus_number; }
  uint32_t port_number() const { return device_info_->port_number; }
  uint8_t device_class() const { return device_info_->class_code; }
  uint8_t device_subclass() const { return device_info_->subclass_code; }
  uint8_t device_protocol() const { return device_info_->protocol_code; }
  uint16_t vendor_id() const { return device_info_->vendor_id; }
  uint16_t product_id() const { return device_info_->product_id; }

  uint16_t usb_version() const;
  uint16_t device_version() const;

  const base::string16& manufacturer_string() const {
    if (device_info_->manufacturer_name)
      return *device_info_->manufacturer_name;

    return base::EmptyString16();
  }
  const base::string16& product_string() const {
    if (device_info_->product_name)
      return *device_info_->product_name;

    return base::EmptyString16();
  }
  const base::string16& serial_number() const {
    if (device_info_->serial_number)
      return *device_info_->serial_number;

    return base::EmptyString16();
  }
  const GURL& webusb_landing_page() const {
    if (device_info_->webusb_landing_page)
      return *device_info_->webusb_landing_page;

    return GURL::EmptyGURL();
  }

  const std::vector<mojom::UsbConfigurationInfoPtr>& configurations() const {
    return device_info_->configurations;
  }
  const mojom::UsbConfigurationInfo* GetActiveConfiguration() const;

  // On ChromeOS the permission_broker service must be used to open USB devices.
  // This function asks it to check whether a future Open call will be allowed.
  // On all other platforms this is a no-op and always returns true.
  virtual void CheckUsbAccess(ResultCallback callback);

  // On Android applications must request permission from the user to access a
  // USB device before it can be opened. After permission is granted the device
  // properties may contain information not previously available. On all other
  // platforms this is a no-op and always returns true.
  virtual void RequestPermission(ResultCallback callback);
  virtual bool permission_granted() const;

  // Creates a UsbDeviceHandle for further manipulation.
  virtual void Open(OpenCallback callback) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class UsbService;

  UsbDevice(uint32_t bus_number, uint32_t port_number);
  explicit UsbDevice(mojom::UsbDeviceInfoPtr device_info);
  UsbDevice(uint16_t usb_version,
            uint8_t device_class,
            uint8_t device_subclass,
            uint8_t device_protocol,
            uint16_t vendor_id,
            uint16_t product_id,
            uint16_t device_version,
            const base::string16& manufacturer_string,
            const base::string16& product_string,
            const base::string16& serial_number,
            uint32_t bus_number,
            uint32_t port_number);
  virtual ~UsbDevice();

  void ActiveConfigurationChanged(int configuration_value);
  void NotifyDeviceRemoved();

  std::list<UsbDeviceHandle*>& handles() { return handles_; }

  // This member must be mutable by subclasses as necessary during device
  // enumeration. To preserve the thread safety of this object they must remain
  // constant afterwards.
  mojom::UsbDeviceInfoPtr device_info_;

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;
  friend class UsbDeviceHandleImpl;
  friend class UsbDeviceHandleUsbfs;
  friend class UsbDeviceHandleWin;
  friend class UsbServiceAndroid;
  friend class UsbServiceImpl;
  friend class UsbServiceLinux;
  friend class UsbServiceWin;

  void OnDisconnect();
  void HandleClosed(UsbDeviceHandle* handle);

  // Weak pointers to open handles. HandleClosed() will be called before each
  // is freed.
  std::list<UsbDeviceHandle*> handles_;

  base::ObserverList<Observer, true>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_H_
