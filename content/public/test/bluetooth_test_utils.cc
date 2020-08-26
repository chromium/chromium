// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/bluetooth_test_utils.h"

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"

namespace content {

void SetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter) {
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
      std::move(adapter));
}

}  // namespace content
