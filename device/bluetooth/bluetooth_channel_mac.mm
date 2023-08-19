// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_channel_mac.h"

#import <IOBluetooth/IOBluetooth.h>

#include "base/check.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"

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
