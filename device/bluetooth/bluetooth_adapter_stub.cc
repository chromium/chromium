// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

// Return a null ptr. Link this when there is no suitable BluetoothAdapter for
// a particular platform.
// static
scoped_refptr<BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  return nullptr;
}

}  // namespace device
