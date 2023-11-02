// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADVERTISEMENT_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADVERTISEMENT_H_

#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {

class MockBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  MockBluetoothAdvertisement();

  MockBluetoothAdvertisement(const MockBluetoothAdvertisement&) = delete;
  MockBluetoothAdvertisement& operator=(const MockBluetoothAdvertisement&) =
      delete;

  // BluetoothAdvertisement overrides:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override;

 private:
  ~MockBluetoothAdvertisement() override;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADVERTISEMENT_H_
