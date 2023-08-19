// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_L2CAP_CHANNEL_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_L2CAP_CHANNEL_MAC_H_

#import <IOBluetooth/IOBluetooth.h>
#import <IOKit/IOReturn.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "device/bluetooth/bluetooth_channel_mac.h"

@class BluetoothL2capChannelDelegate;

namespace device {

class BluetoothL2capChannelMac : public BluetoothChannelMac {
 public:
  // Creates a new L2CAP channel wrapper with the given |socket| and native
  // |channel|.
  BluetoothL2capChannelMac(BluetoothSocketMac* socket,
                           IOBluetoothL2CAPChannel* channel);

  BluetoothL2capChannelMac(const BluetoothL2capChannelMac&) = delete;
  BluetoothL2capChannelMac& operator=(const BluetoothL2capChannelMac&) = delete;

  ~BluetoothL2capChannelMac() override;

  // Opens a new L2CAP channel with Channel ID |channel_id| to the target
  // |device|. Returns the opened channel and sets |status| to kIOReturnSuccess
  // if the open process was successfully started (or if an existing L2CAP
  // channel was found). Otherwise, sets |status| to an error status.
  static std::unique_ptr<BluetoothL2capChannelMac> OpenAsync(
      BluetoothSocketMac* socket,
      IOBluetoothDevice* device,
      BluetoothL2CAPPSM psm,
      IOReturn* status);

  // BluetoothChannelMac:
  void SetSocket(BluetoothSocketMac* socket) override;
  IOBluetoothDevice* GetDevice() override;
  uint16_t GetOutgoingMTU() override;
  IOReturn WriteAsync(void* data, uint16_t length, void* refcon) override;

  void OnChannelOpenComplete(IOBluetoothL2CAPChannel* channel,
                             IOReturn status);
  void OnChannelClosed(IOBluetoothL2CAPChannel* channel);
  void OnChannelDataReceived(IOBluetoothL2CAPChannel* channel,
                             void* data,
                             size_t length);
  void OnChannelWriteComplete(IOBluetoothL2CAPChannel* channel,
                              void* refcon,
                              IOReturn status);

 private:
  // The wrapped native L2CAP channel.
  IOBluetoothL2CAPChannel* __strong channel_;

  // The delegate for the native channel.
  BluetoothL2capChannelDelegate* __strong delegate_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_L2CAP_CHANNEL_MAC_H_
