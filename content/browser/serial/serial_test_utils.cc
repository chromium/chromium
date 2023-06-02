// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/serial/serial_test_utils.h"

#include <utility>

#include "base/functional/callback.h"

namespace content {

MockSerialDelegate::MockSerialDelegate() = default;

MockSerialDelegate::~MockSerialDelegate() = default;

std::unique_ptr<SerialChooser> MockSerialDelegate::RunChooser(
    RenderFrameHost* frame,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
    SerialChooser::Callback callback) {
  std::move(callback).Run(RunChooserInternal());
  return nullptr;
}

SerialTestContentBrowserClient::SerialTestContentBrowserClient() = default;

SerialTestContentBrowserClient::~SerialTestContentBrowserClient() = default;

SerialDelegate* SerialTestContentBrowserClient::GetSerialDelegate() {
  return &delegate_;
}

}  // namespace content
