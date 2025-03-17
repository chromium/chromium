// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_TEST_MOCK_BLUETOOTH_DELEGATE_H_
#define CONTENT_BROWSER_BLUETOOTH_TEST_MOCK_BLUETOOTH_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

class MockBluetoothDelegate : public BluetoothDelegate {
 public:
  MockBluetoothDelegate();
  MockBluetoothDelegate(const MockBluetoothDelegate&) = delete;
  MockBluetoothDelegate& operator=(const MockBluetoothDelegate&) = delete;
  ~MockBluetoothDelegate() override;

  MOCK_METHOD(std::unique_ptr<BluetoothChooser>,
              RunBluetoothChooser,
              (RenderFrameHost*, const BluetoothChooser::EventHandler&));
  MOCK_METHOD(std::unique_ptr<BluetoothScanningPrompt>,
              ShowBluetoothScanningPrompt,
              (RenderFrameHost*, const BluetoothScanningPrompt::EventHandler&));
  MOCK_METHOD(void,
              ShowDevicePairPrompt,
              (RenderFrameHost*,
               const std::u16string&,
               PairPromptCallback,
               PairingKind,
               const std::optional<std::u16string>&));
  MOCK_METHOD(blink::WebBluetoothDeviceId,
              GetWebBluetoothDeviceId,
              (RenderFrameHost*, const std::string&));
  MOCK_METHOD(std::string,
              GetDeviceAddress,
              (RenderFrameHost*, const blink::WebBluetoothDeviceId&));
  MOCK_METHOD(blink::WebBluetoothDeviceId,
              AddScannedDevice,
              (RenderFrameHost*, const std::string&));
  MOCK_METHOD(blink::WebBluetoothDeviceId,
              GrantServiceAccessPermission,
              (RenderFrameHost*,
               const device::BluetoothDevice*,
               const blink::mojom::WebBluetoothRequestDeviceOptions*));
  MOCK_METHOD(bool,
              HasDevicePermission,
              (RenderFrameHost*, const blink::WebBluetoothDeviceId&));
  MOCK_METHOD(void,
              RevokeDevicePermissionWebInitiated,
              (RenderFrameHost*, const blink::WebBluetoothDeviceId& device_id));
  MOCK_METHOD(bool, MayUseBluetooth, (RenderFrameHost*));
  MOCK_METHOD(bool,
              IsAllowedToAccessService,
              (RenderFrameHost*,
               const blink::WebBluetoothDeviceId&,
               const device::BluetoothUUID&));
  MOCK_METHOD(bool,
              IsAllowedToAccessAtLeastOneService,
              (RenderFrameHost*, const blink::WebBluetoothDeviceId&));
  MOCK_METHOD(bool,
              IsAllowedToAccessManufacturerData,
              (RenderFrameHost*, const blink::WebBluetoothDeviceId&, uint16_t));
  MOCK_METHOD(std::vector<blink::mojom::WebBluetoothDevicePtr>,
              GetPermittedDevices,
              (RenderFrameHost*));
  MOCK_METHOD(void, AddFramePermissionObserver, (FramePermissionObserver*));
  MOCK_METHOD(void, RemoveFramePermissionObserver, (FramePermissionObserver*));
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_TEST_MOCK_BLUETOOTH_DELEGATE_H_
