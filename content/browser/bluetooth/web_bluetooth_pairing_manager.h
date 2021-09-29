// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

class WebBluetoothPairingManagerDelegate;

// Manage on-demand pairing for in-flight read/write operations on behalf
// of WebBluetoothServiceImpl.
class CONTENT_EXPORT WebBluetoothPairingManager
    : public device::BluetoothDevice::PairingDelegate {
 public:
  // The maximum number of Bluetooth pairing attempts during a single
  // read/write operation.
  static constexpr int kMaxPairAttempts = 10;

  explicit WebBluetoothPairingManager(
      WebBluetoothPairingManagerDelegate* pairing_manager_delegate);
  ~WebBluetoothPairingManager() override;

  WebBluetoothPairingManager& operator=(const WebBluetoothPairingManager& rhs) =
      delete;
  WebBluetoothPairingManager& operator=(WebBluetoothPairingManager&& rhs) =
      delete;

  // Initiate pairing for the characteristic value specified by
  // |characteristic_instance_id|.
  //
  // If pairing is successful the characteristic value will be read. On success
  // or failure |callback| will be run with the appropriate result.
  void PairForCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback);

  // Initiate pairing for writing the characteristic |value| identified by
  // |characteristic_instance_id|.
  //
  // If pairing is successful the characteristic value will be written.
  // |callback| will be run with the status.
  void PairForCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      blink::mojom::WebBluetoothService::RemoteCharacteristicWriteValueCallback
          callback);

  // Initiate pairing for the descriptor value specified by
  // |descriptor_instance_id|.
  //
  // If pairing is successful the descriptor value will be read. On success
  // or failure |callback| will be run with the appropriate result.
  void PairForDescriptorReadValue(
      const std::string& descriptor_instance_id,
      blink::mojom::WebBluetoothService::RemoteDescriptorReadValueCallback
          read_callback);

  // Initiate pairing for the descriptor value specified by
  // |descriptor_instance_id|.
  //
  // If pairing is successful the descriptor value will be written. On success
  // or failure |callback| will be run with the appropriate result.
  void PairForDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothService::RemoteDescriptorWriteValueCallback
          callback);

 private:
  // Pair the Bluetooth device identified by |device_id|. |num_pair_attempts|
  // represents the number of pairing attempts for the specified device which
  // have been made so for in the current attempt to pair. When done |callback|
  // will be called with the pair status.
  void PairDevice(blink::WebBluetoothDeviceId device_id,
                  int num_pair_attempts,
                  device::BluetoothDevice::ConnectCallback callback);

  // Callback for PairDevice above. If failed due to insufficient
  // authentication another pairing attempt will be performed if the maximum
  // number of pairing attempts has not been reached. Otherwise |callback|
  // will be called.
  void OnPairDevice(
      blink::WebBluetoothDeviceId device_id,
      int num_pair_attempts,
      device::BluetoothDevice::ConnectCallback callback,
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // device::BluetoothPairingDelegate implementation:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // The device IDs currently in the pairing process.
  base::flat_set<blink::WebBluetoothDeviceId> pending_pair_device_ids_;

  // The purpose of WebBluetoothPairingManagerDelegate is to support
  // this class. Currently the WebBluetoothPairingManagerDelegate implementation
  // also owns this class (and thus will outlive it). The contract is that
  // the delegate provider is responsible for ensuring it outlives the
  // manager to which it is provided.
  WebBluetoothPairingManagerDelegate* pairing_manager_delegate_;
  base::WeakPtrFactory<WebBluetoothPairingManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_
