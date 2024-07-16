// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/bluetooth_test_utils.h"

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

namespace content {

void SetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter) {
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
      std::move(adapter));
}

void IgnoreBluetoothVisibilityRequirementsForTesting() {
  WebBluetoothServiceImpl::IgnoreVisibilityRequirementsForTesting();
}

}  // namespace content
