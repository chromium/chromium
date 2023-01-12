// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/usb/usb_service.h"

namespace device {

class UsbDevice;

namespace usb {

// Implements the public Mojo UsbDeviceManager interface by wrapping the
// UsbService instance.
class DeviceManagerImpl : public mojom::UsbDeviceManager,
                          public UsbService::Observer {
 public:
  DeviceManagerImpl();
  // Mostly be used for testing.
  explicit DeviceManagerImpl(std::unique_ptr<UsbService> usb_service);

  DeviceManagerImpl(const DeviceManagerImpl&) = delete;
  DeviceManagerImpl& operator=(const DeviceManagerImpl&) = delete;

  ~DeviceManagerImpl() override;

  void AddReceiver(mojo::PendingReceiver<mojom::UsbDeviceManager> receiver);

  UsbService* GetUsbService() const { return usb_service_.get(); }

 private:
  // mojom::UsbDeviceManager implementation
  void EnumerateDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
      EnumerateDevicesAndSetClientCallback callback) override;
  void GetDevices(mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<mojom::UsbDeviceClient> device_client) override;
  void GetSecurityKeyDevice(
      const std::string& guid,
      mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<mojom::UsbDeviceClient> device_client) override;

#if BUILDFLAG(IS_ANDROID)
  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override;
  void OnPermissionGrantedToRefresh(scoped_refptr<UsbDevice> device,
                                    RefreshDeviceInfoCallback callback,
                                    bool granted);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  void CheckAccess(const std::string& guid,
                   CheckAccessCallback callback) override;

  void OpenFileDescriptor(const std::string& guid,
                          uint32_t drop_privileges_mask,
                          mojo::PlatformHandle lifeline_fd,
                          OpenFileDescriptorCallback callback) override;

  void OnOpenFileDescriptor(OpenFileDescriptorCallback callback,
                            base::ScopedFD fd);

  void OnOpenFileDescriptorError(OpenFileDescriptorCallback callback,
                                 const std::string& error_name,
                                 const std::string& message);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SetClient(mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient>
                     client) override;

  // Callbacks to handle the async responses from the underlying UsbService.
  void OnGetDevices(
      mojom::UsbEnumerationOptionsPtr options,
      mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient> client,
      GetDevicesCallback callback,
      const std::vector<scoped_refptr<UsbDevice>>& devices);

  // UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override;
  void WillDestroyUsbService() override;

  void MaybeRunDeviceChangesCallback();
  void GetDeviceInternal(
      const std::string& guid,
      mojo::PendingReceiver<mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<mojom::UsbDeviceClient> device_client,
      base::span<const uint8_t> blocked_interface_classes,
      bool allow_security_key_requests);

  std::unique_ptr<UsbService> usb_service_;
  base::ScopedObservation<UsbService, UsbService::Observer> observation_{this};

  mojo::ReceiverSet<mojom::UsbDeviceManager> receivers_;
  mojo::AssociatedRemoteSet<mojom::UsbDeviceManagerClient> clients_;

  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_{this};
};

}  // namespace usb
}  // namespace device

#endif  // SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_IMPL_H_
