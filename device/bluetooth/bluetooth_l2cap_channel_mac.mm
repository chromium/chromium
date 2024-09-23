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
// its wrapped `_channel`.
@interface BluetoothL2capChannelDelegate
    : NSObject <IOBluetoothL2CAPChannelDelegate> {
 @private
  raw_ptr<device::BluetoothL2capChannelMac> _channel;  // weak
  IOBluetoothL2CAPChannel* __strong _l2capChannel;

  // While `_l2capChannel` is open, the delegate holds a strong reference to
  // itself to ensure it is not destroyed before l2capChannelClosed is
  // received. This is a workaround for a macOS bug, see Apple Feedback report
  // FB13705522.
  BluetoothL2capChannelDelegate* __strong _strongSelf;
}

- (instancetype)initWithChannel:(device::BluetoothL2capChannelMac*)channel
                   l2capChannel:(IOBluetoothL2CAPChannel*)l2capChannel;

@end

@implementation BluetoothL2capChannelDelegate

- (instancetype)initWithChannel:(device::BluetoothL2capChannelMac*)channel
                   l2capChannel:(IOBluetoothL2CAPChannel*)l2capChannel {
  if ((self = [super init])) {
    _channel = channel;
    _l2capChannel = l2capChannel;
  }

  return self;
}

- (void)l2capChannelOpenComplete:(IOBluetoothL2CAPChannel*)l2capChannel
                          status:(IOReturn)error {
  if (error == kIOReturnSuccess) {
    // Keep the delegate alive until l2capChannelClosed.
    _strongSelf = self;
  }
  if (_channel) {
    _channel->OnChannelOpenComplete(l2capChannel, error);
  }
}

- (void)l2capChannelWriteComplete:(IOBluetoothL2CAPChannel*)l2capChannel
                           refcon:(void*)refcon
                           status:(IOReturn)error {
  if (_channel) {
    _channel->OnChannelWriteComplete(l2capChannel, refcon, error);
  }
}

- (void)l2capChannelData:(IOBluetoothL2CAPChannel*)l2capChannel
                    data:(void*)dataPointer
                  length:(size_t)dataLength {
  if (_channel) {
    _channel->OnChannelDataReceived(l2capChannel, dataPointer, dataLength);
  }
}

- (void)l2capChannelClosed:(IOBluetoothL2CAPChannel*)l2capChannel {
  [_l2capChannel setDelegate:nil];

  // If `_channel` still exists, notify it that the channel was closed so it
  // can release its strong references to `l2capChannel` and the channel
  // delegate (this object). In the typical case we expect `_channel` has
  // already been destroyed.
  if (_channel) {
    _channel->OnChannelClosed(l2capChannel);
  }

  // Remove the last owning references to the channel and delegate. After
  // releasing `_strongSelf` this object may be destroyed, so the only safe
  // thing to do is return.
  _l2capChannel = nil;
  _strongSelf = nil;
}

- (void)resetOwner {
  _channel = nullptr;
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
  // If `channel_` is opened, `delegate_` and `channel_` are allowed to persist
  // until the delegate is notified that the channel has been closed. Reset the
  // delegate's reference to this object so the delegate will not notify us
  // for events that occur after our destruction.
  [delegate_ resetOwner];
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
  delegate_ = [[BluetoothL2capChannelDelegate alloc] initWithChannel:this
                                                        l2capChannel:channel_];
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
  channel_ = nil;
  [delegate_ resetOwner];
  delegate_ = nil;
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
