// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_RFCOMM_CHANNEL_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_RFCOMM_CHANNEL_MAC_H_

#import <IOBluetooth/IOBluetooth.h>
#import <IOKit/IOReturn.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "device/bluetooth/bluetooth_channel_mac.h"

@class BluetoothRfcommChannelDelegate;

namespace device {

class BluetoothRfcommChannelMac : public BluetoothChannelMac {
 public:
  // Creates a new RFCOMM channel wrapper with the given |socket| and native
  // |channel|.
  BluetoothRfcommChannelMac(BluetoothSocketMac* socket,
                            IOBluetoothRFCOMMChannel* channel);

  BluetoothRfcommChannelMac(const BluetoothRfcommChannelMac&) = delete;
  BluetoothRfcommChannelMac& operator=(const BluetoothRfcommChannelMac&) =
      delete;

  ~BluetoothRfcommChannelMac() override;

  // Opens a new RFCOMM channel with Channel ID |channel_id| to the target
  // |device|. Returns the opened channel and sets |status| to kIOReturnSuccess
  // if the open process was successfully started (or if an existing RFCOMM
  // channel was found). Otherwise, sets |status| to an error status.
  static std::unique_ptr<BluetoothRfcommChannelMac> OpenAsync(
      BluetoothSocketMac* socket,
      IOBluetoothDevice* device,
      BluetoothRFCOMMChannelID channel_id,
      IOReturn* status);

  // BluetoothChannelMac:
  void SetSocket(BluetoothSocketMac* socket) override;
  IOBluetoothDevice* GetDevice() override;
  uint16_t GetOutgoingMTU() override;
  IOReturn WriteAsync(void* data, uint16_t length, void* refcon) override;

  void OnChannelOpenComplete(IOBluetoothRFCOMMChannel* channel,
                             IOReturn status);
  void OnChannelClosed(IOBluetoothRFCOMMChannel* channel);
  void OnChannelDataReceived(IOBluetoothRFCOMMChannel* channel,
                             void* data,
                             size_t length);
  void OnChannelWriteComplete(IOBluetoothRFCOMMChannel* channel,
                              void* refcon,
                              IOReturn status);

 private:
  // The wrapped native RFCOMM channel.
  IOBluetoothRFCOMMChannel* __strong channel_;

  // The delegate for the native channel.
  BluetoothRfcommChannelDelegate* __strong delegate_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_RFCOMM_CHANNEL_MAC_H_
