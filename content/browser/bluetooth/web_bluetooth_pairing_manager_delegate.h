// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "device/bluetooth/bluetooth_device.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

// A set of functions needed by whatever object (usually
// WebBluetoothServiceImpl) that embeds the WebBluetoothPairingManager and is
// separated into a separate interface for readability and testing purposes.
class WebBluetoothPairingManagerDelegate {
 public:
  // Return the cached device ID for the given characteric instance ID.
  // The returned device ID may be invalid - check before use.
  virtual blink::WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) = 0;

  // Return the cached device ID for the given descriptor instance ID.
  // The returned device ID may be invalid - check before use.
  virtual blink::WebBluetoothDeviceId GetDescriptorDeviceId(
      const std::string& descriptor_instance_id) = 0;

  // Return the cached device ID for the given |device_address|.
  // The returned device ID may be invalid - check before use.
  virtual blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const std::string& device_address) = 0;

  // Pair the device identified by |device_id|. If successful, |callback| will
  // be run. If unsuccessful |error_callback| wil be run with the corresponding
  // error code.
  virtual void PairDevice(
      const blink::WebBluetoothDeviceId& device_id,
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      device::BluetoothDevice::ConnectCallback callback) = 0;

  // Cancels a pairing attempt to a remote device, clearing its reference to
  // the pairing delegate.
  virtual void CancelPairing(const blink::WebBluetoothDeviceId& device_id) = 0;

  // Sends the PIN code |pincode| for the remote device during pairing.
  virtual void SetPinCode(const blink::WebBluetoothDeviceId& device_id,
                          const std::string& pincode) = 0;

  // The user consented to pairing with the Bluetooth device.
  virtual void PairConfirmed(const blink::WebBluetoothDeviceId& device_id) = 0;

  // Reads the value for the characteristic identified by
  // |characteristic_instance_id|. If the value is successfully read the
  // callback will be run with WebBluetoothResult::SUCCESS and the
  // characteristic's value. If the value is not successfully read the
  // callback will be run with the corresponding error and nullptr for value.
  virtual void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          callback) = 0;

  // Writes the |value| for the characteristic identified by
  // |characteristic_instance_id|. If the value is successfully written
  // |callback| will be run with WebBluetoothResult::SUCCESS. If the value is
  // not successfully written |callback| will be run with the corresponding
  // error.
  virtual void RemoteCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      blink::mojom::WebBluetoothService::RemoteCharacteristicWriteValueCallback
          callback) = 0;

  // Reads the value for the descriptor identified by |descriptor_instance_id|.
  // If successfully read |callback| will be run with
  // WebBluetoothResult::SUCCESS and the descriptor value. If the value is not
  // successfully read the callback will be run with the corresponding error
  // and nullptr for value.
  virtual void RemoteDescriptorReadValue(
      const std::string& descriptor_instance_id,
      blink::mojom::WebBluetoothService::RemoteDescriptorReadValueCallback
          callback) = 0;

  // Writes the |value| for the descriptor identified by
  // |descriptor_instance_id|. If the value is successfully written
  // |callback| will be run with WebBluetoothResult::SUCCESS. If the value is
  // not successfully written |callback| will be run with the corresponding
  // error.
  virtual void RemoteDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothService::RemoteDescriptorWriteValueCallback
          callback) = 0;

  virtual void RemoteCharacteristicStartNotificationsInternal(
      const std::string& characteristic_instance_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      blink::mojom::WebBluetoothService::
          RemoteCharacteristicStartNotificationsCallback callback) = 0;

  // Display a dialog to prompt to user for Bluetooth pairing.
  // |device_identifier| is any string the caller wants to display to the user
  // to identify the device (MAC address, name, etc.). |callback| will be called
  // with the dialog result. |pairng_kind| will be used to determined which
  // prompt to show.
  virtual void PromptForBluetoothPairing(
      const std::u16string& device_identifier,
      BluetoothDelegate::PairPromptCallback callback,
      BluetoothDelegate::PairingKind pairing_kind,
      const std::optional<std::u16string>& pin) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_DELEGATE_H_
