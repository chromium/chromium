// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DEVICE_H_
#define DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DEVICE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

// AndroidAccessoryDevice sends CTAP messages over USB to a given device.
class COMPONENT_EXPORT(DEVICE_FIDO) AndroidAccessoryDevice : public FidoDevice {
 public:
  // These enum values are magic values on the wire that indicate a
  // synchronisation message and a CTAP2 message, respectively.
  enum {
    kCoaoaSync = 119,
    kCoaoaMsg = 33,
  };

  AndroidAccessoryDevice(mojo::Remote<device::mojom::UsbDevice> device,
                         uint8_t in_endpoint,
                         uint8_t out_endpoint);
  ~AndroidAccessoryDevice() override;

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;
  void Cancel(CancelToken token) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  void OnWriteComplete(DeviceCallback callback,
                       device::mojom::UsbTransferStatus result);
  void OnReadLengthComplete(DeviceCallback callback,
                            device::mojom::UsbTransferStatus result,
                            base::span<const uint8_t> payload);
  void OnReadComplete(DeviceCallback callback,
                      const uint32_t length,
                      device::mojom::UsbTransferStatus result,
                      base::span<const uint8_t> payload);

  mojo::Remote<device::mojom::UsbDevice> device_;
  const uint8_t in_endpoint_;
  const uint8_t out_endpoint_;
  uint8_t id_[8];

  std::vector<uint8_t> buffer_;

  base::WeakPtrFactory<AndroidAccessoryDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_AOA_ANDROID_ACCESSORY_DEVICE_H_
