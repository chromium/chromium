// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADVERTISEMENT_OBSERVER_H_
#define DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADVERTISEMENT_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {

// Test implementation of BluetoothAdvertisement::Observer counting method calls
// and caching last reported values.
class TestBluetoothAdvertisementObserver
    : public BluetoothAdvertisement::Observer {
 public:
  explicit TestBluetoothAdvertisementObserver(
      scoped_refptr<BluetoothAdvertisement> advertisement);
  ~TestBluetoothAdvertisementObserver() override;

  // BluetoothAdvertisement::Observer:
  void AdvertisementReleased(BluetoothAdvertisement* advertisement) override;

  bool released() const { return released_; }
  size_t released_count() const { return released_count_; }

 private:
  bool released_ = false;
  size_t released_count_ = 0;
  scoped_refptr<BluetoothAdvertisement> advertisement_;

  DISALLOW_COPY_AND_ASSIGN(TestBluetoothAdvertisementObserver);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADVERTISEMENT_OBSERVER_H_
