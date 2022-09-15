// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace device {

class MockUsbMojoDevice;

// This class implements a fake USB device manager which will only be used in
// tests for device::mojom::UsbDeviceManager's users.
class FakeUsbDeviceManager : public mojom::UsbDeviceManager {
 public:
  using DeviceMap =
      std::unordered_map<std::string, scoped_refptr<FakeUsbDeviceInfo>>;

  FakeUsbDeviceManager();

  FakeUsbDeviceManager(const FakeUsbDeviceManager&) = delete;
  FakeUsbDeviceManager& operator=(const FakeUsbDeviceManager&) = delete;

  ~FakeUsbDeviceManager() override;

  void AddReceiver(mojo::PendingReceiver<mojom::UsbDeviceManager> receiver);

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

  bool SetMockForDevice(const std::string& guid,
                        MockUsbMojoDevice* mock_device);

  bool IsBound() { return !receivers_.empty(); }

  void CloseAllBindings() { receivers_.Clear(); }

  void RemoveAllDevices();

  const device::mojom::UsbDeviceInfo* GetDeviceInfo(const std::string& guid);

  // mojom::UsbDeviceManager implementation:
  void EnumerateDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
      EnumerateDevicesAndSetClientCallback callback) override;
  void GetDevices(mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<mojom::UsbDeviceClient> device_client) override;
  void GetSecurityKeyDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<mojom::UsbDeviceClient> device_client) override;

#if BUILDFLAG(IS_ANDROID)
  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  void CheckAccess(const std::string& guid,
                   CheckAccessCallback callback) override;

  void OpenFileDescriptor(const std::string& guid,
                          uint32_t drop_privileges_mask,
                          mojo::PlatformHandle lifeline_fd,
                          OpenFileDescriptorCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SetClient(mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient>
                     client) override;

 protected:
  DeviceMap& devices() { return devices_; }

  mojo::ReceiverSet<mojom::UsbDeviceManager> receivers_;
  mojo::AssociatedRemoteSet<mojom::UsbDeviceManagerClient> clients_;

  DeviceMap devices_;

  base::WeakPtrFactory<FakeUsbDeviceManager> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_
