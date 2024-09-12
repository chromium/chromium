// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_
#define CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
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
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override;

  void ShowDevicePairPrompt(RenderFrameHost* frame,
                            const std::u16string& device_identifier,
                            PairPromptCallback callback,
                            PairingKind pairing_kind,
                            const std::optional<std::u16string>& pin) override;

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
  void RevokeDevicePermissionWebInitiated(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool MayUseBluetooth(RenderFrameHost* rfh) override;
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override;
  bool IsAllowedToAccessAtLeastOneService(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessManufacturerData(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) override;
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      RenderFrameHost* frame) override;
  void AddFramePermissionObserver(FramePermissionObserver* observer) override;
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override;

 private:
  using AddressToIdMap =
      base::flat_map<std::string, blink::WebBluetoothDeviceId>;
  using OriginPair = std::pair<url::Origin, url::Origin>;
  using IdToServicesMap = base::flat_map<blink::WebBluetoothDeviceId,
                                         base::flat_set<device::BluetoothUUID>>;
  using IdToNameMap = base::flat_map<blink::WebBluetoothDeviceId, std::string>;
  using IdToManufacturerCodesMap =
      base::flat_map<blink::WebBluetoothDeviceId, base::flat_set<uint16_t>>;

  // Finds an existing WebBluetoothDeviceId for |device_address| for |frame| or
  // creates a new ID for the Bluetooth device on the current frame.
  blink::WebBluetoothDeviceId GetOrCreateDeviceIdForDeviceAddress(
      RenderFrameHost* frame,
      const std::string& device_address);

  // Adds the union of |options->filters->services| and
  // |options->optional_services| to the allowed services for |device_id|.
  void GrantUnionOfServicesAndManufacturerDataForDevice(
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
  IdToManufacturerCodesMap device_id_to_manufacturer_code_map_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_FAKE_BLUETOOTH_DELEGATE_H_
