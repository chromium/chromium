// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BLUETOOTH_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BLUETOOTH_TEST_UTILS_H_

#include "base/memory/ref_counted.h"

namespace device {
class BluetoothAdapter;
}

namespace content {

// Configure the BluetoothAdapter which will be returned by
// BluetoothAdapterFactoryWrapper::GetAdapter().
void SetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BLUETOOTH_TEST_UTILS_H_
