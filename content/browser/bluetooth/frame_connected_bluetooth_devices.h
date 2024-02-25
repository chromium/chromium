// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_FRAME_CONNECTED_BLUETOOTH_DEVICES_H_
#define CONTENT_BROWSER_BLUETOOTH_FRAME_CONNECTED_BLUETOOTH_DEVICES_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/origin.h"

namespace device {
class BluetoothGattConnection;
}  // namespace device

namespace content {

struct GATTConnectionAndServerClient;
class RenderFrameHost;
class WebContentsImpl;

// Holds information about connected devices and updates the WebContents
// when new connections are made or connections closed. WebContents must
// outlive this class.
// This class does not keep track of the status of the connections. Owners of
// this class should inform it when a connection is terminated so that the
// connection is removed from the appropriate maps.
class CONTENT_EXPORT FrameConnectedBluetoothDevices final {
 public:
  // |rfh| should be the RenderFrameHost that owns the WebBluetoothServiceImpl
  // that owns this map.
  explicit FrameConnectedBluetoothDevices(RenderFrameHost& rfh);

  FrameConnectedBluetoothDevices(const FrameConnectedBluetoothDevices&) =
      delete;
  FrameConnectedBluetoothDevices& operator=(
      const FrameConnectedBluetoothDevices&) = delete;

  ~FrameConnectedBluetoothDevices();

  // Returns true if the map holds a connection to |device_id|.
  bool IsConnectedToDeviceWithId(const blink::WebBluetoothDeviceId& device_id);

  // If a connection doesn't exist already for |device_id|, adds a connection to
  // the map and increases the WebContents count of connected devices.
  void Insert(
      const blink::WebBluetoothDeviceId& device_id,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client);

  // Deletes the BluetoothGattConnection for |device_id| and decrements the
  // WebContents count of connected devices if |device_id| had a connection.
  void CloseConnectionToDeviceWithId(
      const blink::WebBluetoothDeviceId& device_id);

  // Deletes the BluetoothGattConnection for |device_address| and decrements the
  // WebContents count of connected devices if |device_address| had a
  // connection. Returns the device_id of the device associated with the
  // connection.
  std::optional<blink::WebBluetoothDeviceId> CloseConnectionToDeviceWithAddress(
      const std::string& device_address);

  // Deletes all connections that are NOT in the list of |permitted_ids| and
  // decrements the WebContents count of connected devices for each device that
  // had a connection.
  void CloseConnectionsToDevicesNotInList(
      const std::set<blink::WebBluetoothDeviceId>& permitted_ids);

 private:
  // Increments the Connected Devices count of the frame's WebContents.
  void IncrementDevicesConnectedCount();
  // Decrements the Connected Devices count of the frame's WebContents.
  void DecrementDevicesConnectedCount();

  // WebContentsImpl that owns the WebBluetoothServiceImpl that owns this map.
  raw_ptr<WebContentsImpl> web_contents_impl_;

  // Keeps the BluetoothGattConnection objects alive so that connections don't
  // get closed.
  std::unordered_map<blink::WebBluetoothDeviceId,
                     std::unique_ptr<GATTConnectionAndServerClient>,
                     blink::WebBluetoothDeviceIdHash>
      device_id_to_connection_map_;

  // Keeps track of which device addresses correspond to which ids.
  std::unordered_map<std::string, blink::WebBluetoothDeviceId>
      device_address_to_id_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_FRAME_CONNECTED_BLUETOOTH_DEVICES_H_
