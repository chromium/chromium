// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

struct GATTConnectionAndServerClient {
  GATTConnectionAndServerClient(
      std::unique_ptr<device::BluetoothGattConnection> connection,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client)
      : gatt_connection(std::move(connection)),
        server_client(std::move(client)) {}

  std::unique_ptr<device::BluetoothGattConnection> gatt_connection;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> server_client;
};

FrameConnectedBluetoothDevices::FrameConnectedBluetoothDevices(
    RenderFrameHost& rfh)
    : web_contents_impl_(static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(&rfh))) {}

FrameConnectedBluetoothDevices::~FrameConnectedBluetoothDevices() {
  for (size_t i = 0; i < device_id_to_connection_map_.size(); i++) {
    DecrementDevicesConnectedCount();
  }
}

bool FrameConnectedBluetoothDevices::IsConnectedToDeviceWithId(
    const blink::WebBluetoothDeviceId& device_id) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter == device_id_to_connection_map_.end()) {
    return false;
  }
  DCHECK(connection_iter->second->gatt_connection->IsConnected());
  return true;
}

void FrameConnectedBluetoothDevices::Insert(
    const blink::WebBluetoothDeviceId& device_id,
    std::unique_ptr<device::BluetoothGattConnection> connection,
    mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client) {
  if (device_id_to_connection_map_.find(device_id) !=
      device_id_to_connection_map_.end()) {
    // It's possible for WebBluetoothServiceImpl to issue two successive
    // connection requests for which it would get two successive responses
    // and consequently try to insert two BluetoothGattConnections for the
    // same device. WebBluetoothServiceImpl should reject or queue connection
    // requests if there is a pending connection already, but the platform
    // abstraction doesn't currently support checking for pending connections.
    // TODO(ortuno): CHECK that this never happens once the platform
    // abstraction allows to check for pending connections.
    // http://crbug.com/583544
    return;
  }
  device_address_to_id_map_[connection->GetDeviceAddress()] = device_id;
  auto gatt_connection_and_client =
      std::make_unique<GATTConnectionAndServerClient>(std::move(connection),
                                                      std::move(client));
  device_id_to_connection_map_[device_id] =
      std::move(gatt_connection_and_client);
  IncrementDevicesConnectedCount();
}

void FrameConnectedBluetoothDevices::CloseConnectionToDeviceWithId(
    const blink::WebBluetoothDeviceId& device_id) {
  auto connection_iter = device_id_to_connection_map_.find(device_id);
  if (connection_iter == device_id_to_connection_map_.end()) {
    return;
  }
  CHECK(device_address_to_id_map_.erase(
      connection_iter->second->gatt_connection->GetDeviceAddress()));
  connection_iter->second->server_client->GATTServerDisconnected();
  device_id_to_connection_map_.erase(connection_iter);
  DecrementDevicesConnectedCount();
}

std::optional<blink::WebBluetoothDeviceId>
FrameConnectedBluetoothDevices::CloseConnectionToDeviceWithAddress(
    const std::string& device_address) {
  auto device_address_iter = device_address_to_id_map_.find(device_address);
  if (device_address_iter == device_address_to_id_map_.end()) {
    return std::nullopt;
  }
  blink::WebBluetoothDeviceId device_id = device_address_iter->second;
  auto device_id_iter = device_id_to_connection_map_.find(device_id);
  CHECK(device_id_iter != device_id_to_connection_map_.end());
  device_id_iter->second->server_client->GATTServerDisconnected();
  CHECK(device_address_to_id_map_.erase(device_address));
  device_id_to_connection_map_.erase(device_id);
  DecrementDevicesConnectedCount();
  return std::make_optional(device_id);
}

void FrameConnectedBluetoothDevices::CloseConnectionsToDevicesNotInList(
    const std::set<blink::WebBluetoothDeviceId>& permitted_ids) {
  std::set<blink::WebBluetoothDeviceId> ids_to_delete;

  for (const auto& device_pair : device_id_to_connection_map_) {
    if (!base::Contains(permitted_ids, device_pair.first))
      ids_to_delete.insert(device_pair.first);
  }

  for (const auto& device_id : ids_to_delete)
    CloseConnectionToDeviceWithId(device_id);
}

void FrameConnectedBluetoothDevices::IncrementDevicesConnectedCount() {
  web_contents_impl_->IncrementBluetoothConnectedDeviceCount();
}

void FrameConnectedBluetoothDevices::DecrementDevicesConnectedCount() {
  web_contents_impl_->DecrementBluetoothConnectedDeviceCount();
}

}  // namespace content
