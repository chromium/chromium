// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/test_bluetooth_advertisement_observer.h"

namespace device {

TestBluetoothAdvertisementObserver::TestBluetoothAdvertisementObserver(
    scoped_refptr<BluetoothAdvertisement> advertisement)
    : advertisement_(std::move(advertisement)) {
  advertisement_->AddObserver(this);
}

TestBluetoothAdvertisementObserver::~TestBluetoothAdvertisementObserver() {
  advertisement_->RemoveObserver(this);
}

void TestBluetoothAdvertisementObserver::AdvertisementReleased(
    BluetoothAdvertisement* advertisement) {
  released_ = true;
  ++released_count_;
}

}  // namespace device
