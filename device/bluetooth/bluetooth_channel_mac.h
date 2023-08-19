// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_CHANNEL_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_CHANNEL_MAC_H_

#import <IOKit/IOReturn.h>
#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"

@class IOBluetoothDevice;

namespace device {

class BluetoothSocketMac;

// Wraps a native RFCOMM or L2CAP channel.
class BluetoothChannelMac {
 public:
  BluetoothChannelMac();

  BluetoothChannelMac(const BluetoothChannelMac&) = delete;
  BluetoothChannelMac& operator=(const BluetoothChannelMac&) = delete;

  virtual ~BluetoothChannelMac();

  // Sets the channel's owning socket to |socket|. Should only be called if the
  // socket was previously unset. Note: This can synchronously call back into
  // socket->OnChannelOpenComplete().
  virtual void SetSocket(BluetoothSocketMac* socket);

  // Returns the Bluetooth address for the device associated with |this|
  // channel.
  std::string GetDeviceAddress();

  // Returns the Bluetooth device associated with |this| channel.
  virtual IOBluetoothDevice* GetDevice() = 0;

  // Returns the outgoing MTU (maximum transmission unit) for the channel.
  virtual uint16_t GetOutgoingMTU() = 0;

  // Writes |data| of length |length| bytes into the channel. The |refcon| is a
  // user-supplied value that gets passed to the write callback.
  // Returns kIOReturnSuccess if the data was buffered successfully.
  // If the return value is an error condition none of the data was sent.
  // The number of bytes to be sent must not exceed the channel MTU.
  //
  // Once the data has been successfully passed to the hardware to be
  // transmitted, the socket's method OnChannelWriteComplete() will be called
  // with the |refcon| that was passed to this method.
  virtual IOReturn WriteAsync(void* data, uint16_t length, void* refcon) = 0;

 protected:
  BluetoothSocketMac* socket() { return socket_; }

 private:
  // The socket that owns |this|.
  raw_ptr<BluetoothSocketMac> socket_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_CHANNEL_MAC_H_
