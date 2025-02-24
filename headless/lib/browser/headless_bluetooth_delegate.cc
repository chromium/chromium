// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_bluetooth_delegate.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

namespace headless {

using ::blink::WebBluetoothDeviceId;
using ::content::BluetoothChooser;
using ::content::BluetoothScanningPrompt;
using ::content::RenderFrameHost;
using ::device::BluetoothDevice;
using ::device::BluetoothUUID;

HeadlessBluetoothDelegate::HeadlessBluetoothDelegate() = default;
HeadlessBluetoothDelegate::~HeadlessBluetoothDelegate() = default;

std::unique_ptr<BluetoothChooser>
HeadlessBluetoothDelegate::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return std::make_unique<BluetoothChooser>();
}

std::unique_ptr<BluetoothScanningPrompt>
HeadlessBluetoothDelegate::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  return nullptr;
}

void HeadlessBluetoothDelegate::ShowDevicePairPrompt(
    RenderFrameHost* frame,
    const std::u16string& device_identifier,
    PairPromptCallback callback,
    PairingKind pairing_kind,
    const std::optional<std::u16string>& pin) {}

WebBluetoothDeviceId HeadlessBluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return {};
}

std::string HeadlessBluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return std::string();
}

WebBluetoothDeviceId HeadlessBluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return WebBluetoothDeviceId();
}

WebBluetoothDeviceId HeadlessBluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  return WebBluetoothDeviceId();
}

bool HeadlessBluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return false;
}

void HeadlessBluetoothDelegate::RevokeDevicePermissionWebInitiated(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {}

bool HeadlessBluetoothDelegate::MayUseBluetooth(RenderFrameHost* rfh) {
  return true;
}

bool HeadlessBluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  return false;
}

bool HeadlessBluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return false;
}

bool HeadlessBluetoothDelegate::IsAllowedToAccessManufacturerData(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const uint16_t manufacturer_code) {
  return false;
}

void HeadlessBluetoothDelegate::AddFramePermissionObserver(
    FramePermissionObserver* observer) {}

void HeadlessBluetoothDelegate::RemoveFramePermissionObserver(
    FramePermissionObserver* observer) {}

std::vector<blink::mojom::WebBluetoothDevicePtr>
HeadlessBluetoothDelegate::GetPermittedDevices(RenderFrameHost* frame) {
  return {};
}

}  // namespace headless
