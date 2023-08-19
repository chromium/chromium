// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_l2cap_channel_mac.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

// A simple delegate class for an open L2CAP channel that forwards methods to
// its wrapped |channel_|.
@interface BluetoothL2capChannelDelegate
    : NSObject <IOBluetoothL2CAPChannelDelegate> {
 @private
  raw_ptr<device::BluetoothL2capChannelMac> _channel;  // weak
}

- (instancetype)initWithChannel:(device::BluetoothL2capChannelMac*)channel;

@end

@implementation BluetoothL2capChannelDelegate

- (instancetype)initWithChannel:(device::BluetoothL2capChannelMac*)channel {
  if ((self = [super init]))
    _channel = channel;

  return self;
}

- (void)l2capChannelOpenComplete:(IOBluetoothL2CAPChannel*)l2capChannel
                          status:(IOReturn)error {
  _channel->OnChannelOpenComplete(l2capChannel, error);
}

- (void)l2capChannelWriteComplete:(IOBluetoothL2CAPChannel*)l2capChannel
                           refcon:(void*)refcon
                           status:(IOReturn)error {
  _channel->OnChannelWriteComplete(l2capChannel, refcon, error);
}

- (void)l2capChannelData:(IOBluetoothL2CAPChannel*)l2capChannel
                    data:(void*)dataPointer
                  length:(size_t)dataLength {
  _channel->OnChannelDataReceived(l2capChannel, dataPointer, dataLength);
}

- (void)l2capChannelClosed:(IOBluetoothL2CAPChannel*)l2capChannel {
  _channel->OnChannelClosed(l2capChannel);
}

@end

namespace device {

BluetoothL2capChannelMac::BluetoothL2capChannelMac(
    BluetoothSocketMac* socket,
    IOBluetoothL2CAPChannel* channel)
    : channel_(channel),
      delegate_(nil) {
  SetSocket(socket);
}

BluetoothL2capChannelMac::~BluetoothL2capChannelMac() {
  [channel_ setDelegate:nil];
  [channel_ closeChannel];
}

// static
std::unique_ptr<BluetoothL2capChannelMac> BluetoothL2capChannelMac::OpenAsync(
    BluetoothSocketMac* socket,
    IOBluetoothDevice* device,
    BluetoothL2CAPPSM psm,
    IOReturn* status) {
  DCHECK(socket);
  std::unique_ptr<BluetoothL2capChannelMac> channel(
      new BluetoothL2capChannelMac(socket, /*channel=*/nil));

  DCHECK(channel->delegate_);
  IOBluetoothL2CAPChannel* l2cap_channel;
  *status = [device openL2CAPChannelAsync:&l2cap_channel
                                  withPSM:psm
                                 delegate:channel->delegate_];
  if (*status == kIOReturnSuccess)
    channel->channel_ = l2cap_channel;
  else
    channel.reset();

  return channel;
}

void BluetoothL2capChannelMac::SetSocket(BluetoothSocketMac* socket) {
  BluetoothChannelMac::SetSocket(socket);
  if (!this->socket())
    return;

  // Now that the socket is set, it's safe to associate a delegate, which can
  // call back to the socket.
  DCHECK(!delegate_);
  delegate_ = [[BluetoothL2capChannelDelegate alloc] initWithChannel:this];
  [channel_ setDelegate:delegate_];
}

IOBluetoothDevice* BluetoothL2capChannelMac::GetDevice() {
  return [channel_ device];
}

uint16_t BluetoothL2capChannelMac::GetOutgoingMTU() {
  return [channel_ outgoingMTU];
}

IOReturn BluetoothL2capChannelMac::WriteAsync(void* data,
                                              uint16_t length,
                                              void* refcon) {
  DCHECK_LE(length, GetOutgoingMTU());
  return [channel_ writeAsync:data length:length refcon:refcon];
}

void BluetoothL2capChannelMac::OnChannelOpenComplete(
    IOBluetoothL2CAPChannel* channel,
    IOReturn status) {
  if (channel_) {
    DCHECK_EQ(channel_, channel);
  } else {
    // The (potentially) asynchronous connection occurred synchronously.
    // Should only be reachable from OpenAsync().
    DCHECK_EQ(status, kIOReturnSuccess);
  }

  socket()->OnChannelOpenComplete(
      BluetoothClassicDeviceMac::GetDeviceAddress([channel device]), status);
}

void BluetoothL2capChannelMac::OnChannelClosed(
    IOBluetoothL2CAPChannel* channel) {
  DCHECK_EQ(channel_, channel);
  socket()->OnChannelClosed();
}

void BluetoothL2capChannelMac::OnChannelDataReceived(
    IOBluetoothL2CAPChannel* channel,
    void* data,
    size_t length) {
  DCHECK_EQ(channel_, channel);
  socket()->OnChannelDataReceived(data, length);
}

void BluetoothL2capChannelMac::OnChannelWriteComplete(
    IOBluetoothL2CAPChannel* channel,
    void* refcon,
    IOReturn status) {
  // Note: We use "CHECK" below to ensure we never run into unforeseen
  // occurrences of asynchronous callbacks, which could lead to data
  // corruption.
  CHECK_EQ(channel_, channel);
  socket()->OnChannelWriteComplete(refcon, status);
}

}  // namespace device
