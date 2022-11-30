// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_advertisement.h"

namespace device {

MockBluetoothAdvertisement::MockBluetoothAdvertisement() = default;

MockBluetoothAdvertisement::~MockBluetoothAdvertisement() = default;

void MockBluetoothAdvertisement::Unregister(SuccessCallback success_callback,
                                            ErrorCallback error_callback) {
  std::move(success_callback).Run();
}

}  // namespace device
