// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_
#define CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

namespace blink {
class WebBluetoothDeviceId;
}  // namespace blink

namespace device {
class BluetoothDevice;
class BluetoothUUID;
}  // namespace device

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;

// Fakes Web Bluetooth permissions for web tests by emulating Chrome's
// implementation.
class FakeBluetoothDelegate : public BluetoothDelegate {
 public:
  FakeBluetoothDelegate();
  ~FakeBluetoothDelegate() override;

  // Move-only class.
  FakeBluetoothDelegate(const FakeBluetoothDelegate&) = delete;
  FakeBluetoothDelegate& operator=(const FakeBluetoothDelegate&) = delete;

  // BluetoothDelegate implementation:
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      RenderFrameHost* frame,
      const std::string& device_address) override;
  std::string GetDeviceAddress(RenderFrameHost* frame,
                               const blink::WebBluetoothDeviceId&) override;
  blink::WebBluetoothDeviceId AddScannedDevice(
      RenderFrameHost* frame,
      const std::string& device_address) override;
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override;
  bool HasDevicePermission(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override;
  bool IsAllowedToAccessAtLeastOneService(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      RenderFrameHost* frame) override;

 private:
  using AddressToIdMap = std::map<std::string, blink::WebBluetoothDeviceId>;
  using OriginPair = std::pair<url::Origin, url::Origin>;
  using IdToServicesMap = std::map<blink::WebBluetoothDeviceId,
                                   base::flat_set<device::BluetoothUUID>>;
  using IdToNameMap = std::map<blink::WebBluetoothDeviceId, std::string>;

  // Finds an existing WebBluetoothDeviceId for |device_address| for |frame| or
  // creates a new ID for the Bluetooth device on the current frame.
  blink::WebBluetoothDeviceId GetOrCreateDeviceIdForDeviceAddress(
      RenderFrameHost* frame,
      const std::string& device_address);

  // Adds the union of |options->filters->services| and
  // |options->optional_services| to the allowed services for |device_id|.
  void GrantUnionOfServicesForDevice(
      const blink::WebBluetoothDeviceId& device_id,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options);
  AddressToIdMap& GetAddressToIdMapForOrigin(RenderFrameHost* frame);

  // Maps origins to their own maps of device address to device ID.
  // If a given origin and device address does not have an associated device ID,
  // then the origin does not have permission to access the device.
  std::map<OriginPair, AddressToIdMap> device_address_to_id_map_for_origin_;

  // These map device IDs to their set of allowed services and device names.
  // Since devices IDs are randomly generated, it is very unlikely that two
  // unique devices will share the same ID. Therefore, these maps contain all of
  // the service permissions and device names from all of the origins.
  IdToServicesMap device_id_to_services_map_;
  IdToNameMap device_id_to_name_map_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_
