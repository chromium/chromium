// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DISCOVERY_H_
#define DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DISCOVERY_H_

#include <array>
#include <memory>
#include <tuple>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device_discovery.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace device {

// AndroidAccessoryDiscovery watches for USB devices that are inserted during
// its lifetime and tries sending AOA[1] commands to them in case they are a
// phone that can speak CTAP over the accessory protocol.
//
// [1] https://source.android.com/devices/accessories/aoa
class COMPONENT_EXPORT(DEVICE_FIDO) AndroidAccessoryDiscovery
    : public FidoDeviceDiscovery,
      device::mojom::UsbDeviceManagerClient {
 public:
  // InterfaceInfo contains the results of evaluating the USB metadata from an
  // accessory device.
  struct InterfaceInfo {
    // configuration is the USB configuration number that contains the AOA
    // interface.
    uint8_t configuration;
    // interface is the interface number of the AOA interface.
    uint8_t interface;
    // in_endpoint and out_endpoint are the endpoint numbers for AOA.
    uint8_t in_endpoint;
    uint8_t out_endpoint;
    // guid is the identifier assigned by Chromium's USB layer to this specific
    // USB connection.
    std::string guid;
  };

  // The |request_description| is a string that is sent to the device to
  // describe the type of request and may appears in permissions UI on the
  // device.
  AndroidAccessoryDiscovery(mojo::Remote<device::mojom::UsbDeviceManager>,
                            std::string request_description);

  AndroidAccessoryDiscovery(const AndroidAccessoryDiscovery&) = delete;
  AndroidAccessoryDiscovery& operator=(const AndroidAccessoryDiscovery&) =
      delete;

  ~AndroidAccessoryDiscovery() override;

 private:
  static constexpr size_t kSyncNonceLength = 16;
  static constexpr size_t kSyncMessageLength =
      sizeof(uint8_t) + AndroidAccessoryDiscovery::kSyncNonceLength;

  // FidoDeviceDiscovery:
  void StartInternal() override;

  // device::mojom::UsbDeviceManagerClient:
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  void OnGetDevices(std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  void OnOpen(mojo::Remote<device::mojom::UsbDevice> device,
              device::mojom::UsbOpenDeviceError error);
  void OnVersionReply(mojo::Remote<device::mojom::UsbDevice> device,
                      device::mojom::UsbTransferStatus status,
                      base::span<const uint8_t> payload);
  void OnConfigurationStepComplete(
      mojo::Remote<device::mojom::UsbDevice> device,
      unsigned step,
      device::mojom::UsbTransferStatus status);

  void HandleAccessoryDevice(mojo::Remote<device::mojom::UsbDevice> device,
                             device::mojom::UsbDeviceInfoPtr device_info);
  void OnAccessoryConfigured(mojo::Remote<device::mojom::UsbDevice> device,
                             InterfaceInfo interface_info,
                             bool success);
  void OnOpenAccessory(mojo::Remote<device::mojom::UsbDevice> device,
                       device::mojom::UsbDeviceInfoPtr device_info,
                       InterfaceInfo interface_info,
                       device::mojom::UsbOpenDeviceError error);
  void OnSyncWritten(mojo::Remote<device::mojom::UsbDevice> device,
                     InterfaceInfo interface_info,
                     std::array<uint8_t, kSyncNonceLength> nonce,
                     mojom::UsbTransferStatus result);
  void OnReadComplete(mojo::Remote<device::mojom::UsbDevice> device,
                      InterfaceInfo interface_info,
                      std::array<uint8_t, kSyncNonceLength> nonce,
                      mojom::UsbTransferStatus result,
                      base::span<const uint8_t> payload);
  void OnAccessoryInterfaceClaimed(
      mojo::Remote<device::mojom::UsbDevice> device,
      InterfaceInfo interface_info,
      mojom::UsbClaimInterfaceResult result);

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  const std::string request_description_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient> receiver_{
      this};
  base::WeakPtrFactory<AndroidAccessoryDiscovery> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DISCOVERY_H_
