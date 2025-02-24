// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_

#include <string>
#include <vector>

#include "content/public/browser/bluetooth_delegate.h"
#include "headless/public/headless_export.h"

namespace headless {

// A thin layer of BluetoothDelegate for Headless shell that provides a basic
// chooser and rejects any permission of accessing a bluetooth device.
class HEADLESS_EXPORT HeadlessBluetoothDelegate
    : public content::BluetoothDelegate {
 public:
  HeadlessBluetoothDelegate();
  // Not copyable or movable.
  HeadlessBluetoothDelegate(const HeadlessBluetoothDelegate&) = delete;
  HeadlessBluetoothDelegate& operator=(const HeadlessBluetoothDelegate&) =
      delete;
  ~HeadlessBluetoothDelegate() override;

  // BluetoothDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;
  void ShowDevicePairPrompt(content::RenderFrameHost* frame,
                            const std::u16string& device_identifier,
                            PairPromptCallback callback,
                            PairingKind pairing_kind,
                            const std::optional<std::u16string>& pin) override;
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  std::string GetDeviceAddress(content::RenderFrameHost* frame,
                               const blink::WebBluetoothDeviceId&) override;
  blink::WebBluetoothDeviceId AddScannedDevice(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      content::RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override;
  bool HasDevicePermission(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  void RevokeDevicePermissionWebInitiated(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool MayUseBluetooth(content::RenderFrameHost* rfh) override;
  bool IsAllowedToAccessService(content::RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override;
  bool IsAllowedToAccessAtLeastOneService(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessManufacturerData(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) override;
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      content::RenderFrameHost* frame) override;
  void AddFramePermissionObserver(FramePermissionObserver* observer) override;
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BLUETOOTH_DELEGATE_H_
