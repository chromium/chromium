// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_IMPL_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_IMPL_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_device.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

class CONTENT_EXPORT WebBluetoothPairingManagerImpl
    : public WebBluetoothPairingManager,
      public device::BluetoothDevice::PairingDelegate {
 public:
  // The maximum number of Bluetooth pairing attempts during a single
  // read/write operation.
  static constexpr int kMaxPairAttempts = 10;

  // Passkey/Pin has to be exact 6 digits
  static constexpr int kPairingPinSize = 6;

  explicit WebBluetoothPairingManagerImpl(
      WebBluetoothPairingManagerDelegate* pairing_manager_delegate);
  ~WebBluetoothPairingManagerImpl() override;

  WebBluetoothPairingManagerImpl& operator=(
      const WebBluetoothPairingManagerImpl& rhs) = delete;
  WebBluetoothPairingManagerImpl& operator=(
      WebBluetoothPairingManagerImpl&& rhs) = delete;

  // WebBluetoothPairingManager implementation:
  void PairForCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback) override;
  void PairForCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      blink::mojom::WebBluetoothService::RemoteCharacteristicWriteValueCallback
          callback) override;
  void PairForDescriptorReadValue(
      const std::string& descriptor_instance_id,
      blink::mojom::WebBluetoothService::RemoteDescriptorReadValueCallback
          read_callback) override;
  void PairForDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothService::RemoteDescriptorWriteValueCallback
          callback) override;
  void PairForCharacteristicStartNotifications(
      const std::string& characteristic_instance_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      blink::mojom::WebBluetoothService::
          RemoteCharacteristicStartNotificationsCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           CredentialPromptPINSuccess);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           CredentialPromptPINCancelled);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           CredentialPromptPasskeyCancelled);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           PairConfirmPromptSuccess);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           PairConfirmPromptCancelled);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           PairConfirmPinPromptSuccess);
  FRIEND_TEST_ALL_PREFIXES(BluetoothPairingManagerTest,
                           PairConfirmPinPromptCancelled);

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
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  void OnPinCodeResult(blink::WebBluetoothDeviceId device_id,
                       const BluetoothDelegate::PairPromptResult& result);

  void OnPairConfirmResult(blink::WebBluetoothDeviceId device_id,
                           const BluetoothDelegate::PairPromptResult& result);

  // device::BluetoothDevice::PairingDelegate implementation:
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
  // this class. Currently the WebBluetoothPairingManagerDelegate
  // implementation also owns this class (and thus will outlive it). The
  // contract is that the delegate provider is responsible for ensuring it
  // outlives the manager to which it is provided.
  raw_ptr<WebBluetoothPairingManagerDelegate> pairing_manager_delegate_;
  base::WeakPtrFactory<WebBluetoothPairingManagerImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_IMPL_H_
