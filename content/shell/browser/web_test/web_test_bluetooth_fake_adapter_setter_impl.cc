// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_bluetooth_fake_adapter_setter_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/test/web_test_support.h"
#include "content/shell/browser/web_test/web_test_bluetooth_adapter_provider.h"
#include "content/shell/common/web_test/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

WebTestBluetoothFakeAdapterSetterImpl::WebTestBluetoothFakeAdapterSetterImpl() {
}

WebTestBluetoothFakeAdapterSetterImpl::
    ~WebTestBluetoothFakeAdapterSetterImpl() {}

// static
void WebTestBluetoothFakeAdapterSetterImpl::Create(
    mojo::PendingReceiver<mojom::WebTestBluetoothFakeAdapterSetter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebTestBluetoothFakeAdapterSetterImpl>(),
      std::move(receiver));
}

void WebTestBluetoothFakeAdapterSetterImpl::Set(const std::string& adapter_name,
                                                SetCallback callback) {
  SetTestBluetoothScanDuration(
      BluetoothTestScanDurationSetting::kImmediateTimeout);

  device::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
      WebTestBluetoothAdapterProvider::GetBluetoothAdapter(adapter_name));

  std::move(callback).Run();
}

}  // namespace content
