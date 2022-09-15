// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_bluetooth_fake_adapter_setter_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/public/test/bluetooth_test_utils.h"
#include "content/web_test/browser/web_test_bluetooth_adapter_provider.h"
#include "content/web_test/common/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

WebTestBluetoothFakeAdapterSetterImpl::WebTestBluetoothFakeAdapterSetterImpl() =
    default;

WebTestBluetoothFakeAdapterSetterImpl::
    ~WebTestBluetoothFakeAdapterSetterImpl() = default;

// static
void WebTestBluetoothFakeAdapterSetterImpl::Create(
    mojo::PendingReceiver<mojom::WebTestBluetoothFakeAdapterSetter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebTestBluetoothFakeAdapterSetterImpl>(),
      std::move(receiver));
}

void WebTestBluetoothFakeAdapterSetterImpl::Set(const std::string& adapter_name,
                                                SetCallback callback) {
  BluetoothDeviceChooserController::SetTestScanDurationForTesting(
      BluetoothDeviceChooserController::TestScanDurationSetting::
          IMMEDIATE_TIMEOUT);

  SetBluetoothAdapter(
      WebTestBluetoothAdapterProvider::GetBluetoothAdapter(adapter_name));

  std::move(callback).Run();
}

}  // namespace content
