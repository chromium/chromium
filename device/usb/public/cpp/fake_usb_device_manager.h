// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_MANAGER_H_
#define DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_MANAGER_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "device/usb/public/cpp/fake_usb_device_info.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace device {

// This class implements a fake USB device manager which will only be used in
// tests for device::mojom::UsbDeviceManager's users.
class FakeUsbDeviceManager : public mojom::UsbDeviceManager {
 public:
  FakeUsbDeviceManager();
  ~FakeUsbDeviceManager() override;

  void AddBinding(mojom::UsbDeviceManagerRequest request);

  // Create a device and add it to added_devices_.
  template <typename... Args>
  mojom::UsbDeviceInfoPtr CreateAndAddDevice(Args&&... args) {
    scoped_refptr<FakeUsbDeviceInfo> device =
        new FakeUsbDeviceInfo(std::forward<Args>(args)...);
    return AddDevice(device);
  }

  mojom::UsbDeviceInfoPtr AddDevice(scoped_refptr<FakeUsbDeviceInfo> device);

  void RemoveDevice(const std::string& guid);

  void RemoveDevice(scoped_refptr<FakeUsbDeviceInfo> device);

  bool IsBound() { return !bindings_.empty(); }

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

 private:
  // mojom::UsbDeviceManager implementation:
  void GetDevices(mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(const std::string& guid,
                 mojom::UsbDeviceRequest device_request,
                 mojom::UsbDeviceClientPtr device_client) override;
  void SetClient(
      mojom::UsbDeviceManagerClientAssociatedPtrInfo client) override;

  mojo::BindingSet<mojom::UsbDeviceManager> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::UsbDeviceManagerClient> clients_;

  std::unordered_map<std::string, scoped_refptr<FakeUsbDeviceInfo>> devices_;

  base::WeakPtrFactory<FakeUsbDeviceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeUsbDeviceManager);
};

}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_FAKE_USB_DEVICE_MANAGER_H_
