// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/bluetooth_delegate.h"

#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

namespace content {

std::unique_ptr<BluetoothChooser> BluetoothDelegate::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

std::unique_ptr<BluetoothScanningPrompt>
BluetoothDelegate::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  return nullptr;
}

blink::WebBluetoothDeviceId BluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return blink::WebBluetoothDeviceId();
}

std::string BluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const blink::WebBluetoothDeviceId& device_id) {
  return std::string();
}

blink::WebBluetoothDeviceId BluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return blink::WebBluetoothDeviceId();
}

blink::WebBluetoothDeviceId BluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  return blink::WebBluetoothDeviceId();
}

bool BluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const blink::WebBluetoothDeviceId& device_id) {
  return false;
}

bool BluetoothDelegate::MayUseBluetooth(RenderFrameHost* frame) {
  return true;
}

bool BluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const blink::WebBluetoothDeviceId& device_id,
    const device::BluetoothUUID& service) {
  return false;
}

bool BluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const blink::WebBluetoothDeviceId& device_id) {
  return false;
}

bool BluetoothDelegate::IsAllowedToAccessManufacturerData(
    RenderFrameHost* frame,
    const blink::WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  return false;
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
BluetoothDelegate::GetPermittedDevices(RenderFrameHost* frame) {
  return {};
}

BluetoothDelegate::AllowWebBluetoothResult BluetoothDelegate::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return BluetoothDelegate::AllowWebBluetoothResult::kAllow;
}

std::string BluetoothDelegate::GetWebBluetoothBlocklist() {
  return std::string();
}

bool BluetoothDelegate::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return false;
}

}  // namespace content
