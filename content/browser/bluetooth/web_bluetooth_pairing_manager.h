// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_device.h"
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
  // |characteristic_instance_id|. |num_pair_attempts| represents the number of
  // attempts at pairing that have been made so far for this read operation.
  //
  // If pairing is successful the characteristic value will be read. Success or
  // failure |read_callback| will be run with the appropriate result.
  void PairForCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      int num_pair_attempts,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback);

 private:
  void OnReadCharacteristicValuePairSuccess(
      std::string characteristic_instance_id,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback);

  void OnReadCharacteristicValuePairFailure(
      std::string characteristic_instance_id,
      int num_pair_attempts,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback,
      device::BluetoothDevice::ConnectErrorCode error_code);

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
