// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_channel_mac.h"

#import <IOBluetooth/IOBluetooth.h>

#include "base/check.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace device {

BluetoothChannelMac::BluetoothChannelMac() : socket_(nullptr) {}

BluetoothChannelMac::~BluetoothChannelMac() = default;

void BluetoothChannelMac::SetSocket(BluetoothSocketMac* socket) {
  DCHECK(!socket_);
  socket_ = socket;
}

std::string BluetoothChannelMac::GetDeviceAddress() {
  return BluetoothClassicDeviceMac::GetDeviceAddress(GetDevice());
}

}  // namespace device
