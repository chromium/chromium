// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

// Manage on-demand pairing for in-flight read/write operations on behalf
// of the calling client.
class WebBluetoothPairingManager {
 public:
  WebBluetoothPairingManager() = default;
  virtual ~WebBluetoothPairingManager() = default;

  WebBluetoothPairingManager& operator=(const WebBluetoothPairingManager& rhs) =
      delete;
  WebBluetoothPairingManager& operator=(WebBluetoothPairingManager&& rhs) =
      delete;

  // Initiate pairing for the characteristic value specified by
  // |characteristic_instance_id|.
  //
  // If pairing is successful the characteristic value will be read. On success
  // or failure |callback| will be run with the appropriate result.
  virtual void PairForCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      blink::mojom::WebBluetoothService::RemoteCharacteristicReadValueCallback
          read_callback) = 0;

  // Initiate pairing for writing the characteristic |value| identified by
  // |characteristic_instance_id|.
  //
  // If pairing is successful the characteristic value will be written.
  // |callback| will be run with the status.
  virtual void PairForCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      blink::mojom::WebBluetoothService::RemoteCharacteristicWriteValueCallback
          callback) = 0;

  // Initiate pairing for the descriptor value specified by
  // |descriptor_instance_id|.
  //
  // If pairing is successful the descriptor value will be read. On success
  // or failure |callback| will be run with the appropriate result.
  virtual void PairForDescriptorReadValue(
      const std::string& descriptor_instance_id,
      blink::mojom::WebBluetoothService::RemoteDescriptorReadValueCallback
          read_callback) = 0;

  // Initiate pairing for the descriptor value specified by
  // |descriptor_instance_id|.
  //
  // If pairing is successful the descriptor value will be written. On success
  // or failure |callback| will be run with the appropriate result.
  virtual void PairForDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothService::RemoteDescriptorWriteValueCallback
          callback) = 0;

  // Initiate pairing for the characteristic value specified by
  // |characteristic_instance_id|.
  //
  // If pairing is successful the attempt to start characteristic notifications
  // will be reattempted. On success or failure |callback| will be run with the
  // appropriate result.
  virtual void PairForCharacteristicStartNotifications(
      const std::string& characteristic_instance_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      blink::mojom::WebBluetoothService::
          RemoteCharacteristicStartNotificationsCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_PAIRING_MANAGER_H_
