// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_rfcomm_channel_mac.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

// A simple delegate class for an open RFCOMM channel that forwards methods to
// its wrapped `_channel`.
@interface BluetoothRfcommChannelDelegate
    : NSObject <IOBluetoothRFCOMMChannelDelegate> {
 @private
  raw_ptr<device::BluetoothRfcommChannelMac> _channel;  // weak
  IOBluetoothRFCOMMChannel* __strong _rfcommChannel;

  // While `_rfcommChannel` is open, the delegate holds a strong reference to
  // itself to ensure it is not destroyed before rfcommChannelClosed is
  // received. This is a workaround for a macOS bug, see Apple Feedback report
  // FB13705522.
  BluetoothRfcommChannelDelegate* __strong _strongSelf;
}

- (instancetype)initWithChannel:(device::BluetoothRfcommChannelMac*)channel
                  rfcommChannel:(IOBluetoothRFCOMMChannel*)rfcommChannel;
- (void)setRfcommChannel:(IOBluetoothRFCOMMChannel*)rfcommChannel;

@end

@implementation BluetoothRfcommChannelDelegate

- (instancetype)initWithChannel:(device::BluetoothRfcommChannelMac*)channel
                  rfcommChannel:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  if ((self = [super init])) {
    _channel = channel;
    _rfcommChannel = rfcommChannel;
  }

  return self;
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                           status:(IOReturn)error {
  CHECK(_rfcommChannel);
  if (error == kIOReturnSuccess) {
    // Keep the delegate alive until rfcommChannelClosed.
    _strongSelf = self;
  }
  if (_channel) {
    _channel->OnChannelOpenComplete(rfcommChannel, error);
  }
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                            refcon:(void*)refcon
                            status:(IOReturn)error {
  if (_channel) {
    _channel->OnChannelWriteComplete(rfcommChannel, refcon, error);
  }
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  if (_channel) {
    _channel->OnChannelDataReceived(rfcommChannel, dataPointer, dataLength);
  }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  [_rfcommChannel setDelegate:nil];

  // If `_channel` still exists, notify it that the channel was closed so it
  // can release its strong references to `rfcommChannel` and the channel
  // delegate (this object). In the typical case we expect `_channel` has
  // already been destroyed.
  if (_channel) {
    _channel->OnChannelClosed(rfcommChannel);
  }

  // Remove the last owning references to the channel and delegate. After
  // releasing `_strongSelf` this object may be destroyed, so the only safe
  // thing to do is return.
  _rfcommChannel = nil;
  _strongSelf = nil;
}

- (void)resetOwner {
  _channel = nullptr;
}

- (void)setRfcommChannel:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  CHECK(!_rfcommChannel);
  _rfcommChannel = rfcommChannel;
}

@end

namespace device {

BluetoothRfcommChannelMac::BluetoothRfcommChannelMac(
    BluetoothSocketMac* socket,
    IOBluetoothRFCOMMChannel* channel)
    : channel_(channel),
      delegate_(nil) {
  SetSocket(socket);
}

BluetoothRfcommChannelMac::~BluetoothRfcommChannelMac() {
  // If `channel_` is opened, `delegate_` and `channel_` are allowed to persist
  // until the delegate is notified that the channel has been closed. Reset the
  // delegate's reference to this object so the delegate will not notify us
  // for events that occur after our destruction.
  [delegate_ resetOwner];
  [channel_ closeChannel];
}

// static
std::unique_ptr<BluetoothRfcommChannelMac> BluetoothRfcommChannelMac::OpenAsync(
    BluetoothSocketMac* socket,
    IOBluetoothDevice* device,
    BluetoothRFCOMMChannelID channel_id,
    IOReturn* status) {
  DCHECK(socket);
  std::unique_ptr<BluetoothRfcommChannelMac> channel(
      new BluetoothRfcommChannelMac(socket, /*channel=*/nil));

  DCHECK(channel->delegate_);
  IOBluetoothRFCOMMChannel* rfcomm_channel;
  *status = [device openRFCOMMChannelAsync:&rfcomm_channel
                             withChannelID:channel_id
                                  delegate:channel->delegate_];
  if (*status == kIOReturnSuccess) {
    channel->channel_ = rfcomm_channel;
    [channel->delegate_ setRfcommChannel:rfcomm_channel];
  } else {
    channel.reset();
  }

  return channel;
}

void BluetoothRfcommChannelMac::SetSocket(BluetoothSocketMac* socket) {
  BluetoothChannelMac::SetSocket(socket);
  if (!this->socket())
    return;

  // Now that the socket is set, it's safe to associate a delegate, which can
  // call back to the socket.
  DCHECK(!delegate_);
  delegate_ = [[BluetoothRfcommChannelDelegate alloc] initWithChannel:this
                                                        rfcommChannel:channel_];
  [channel_ setDelegate:delegate_];
}

IOBluetoothDevice* BluetoothRfcommChannelMac::GetDevice() {
  return [channel_ getDevice];
}

uint16_t BluetoothRfcommChannelMac::GetOutgoingMTU() {
  return [channel_ getMTU];
}

IOReturn BluetoothRfcommChannelMac::WriteAsync(void* data,
                                               uint16_t length,
                                               void* refcon) {
  DCHECK_LE(length, GetOutgoingMTU());
  return [channel_ writeAsync:data length:length refcon:refcon];
}

void BluetoothRfcommChannelMac::OnChannelOpenComplete(
    IOBluetoothRFCOMMChannel* channel,
    IOReturn status) {
  if (channel_) {
    DCHECK_EQ(channel_, channel);
  } else {
    // The (potentially) asynchronous connection occurred synchronously.
    // Should only be reachable from OpenAsync().
    DCHECK_EQ(status, kIOReturnSuccess);
  }

  socket()->OnChannelOpenComplete(
      BluetoothClassicDeviceMac::GetDeviceAddress([channel getDevice]), status);
}

void BluetoothRfcommChannelMac::OnChannelClosed(
    IOBluetoothRFCOMMChannel* channel) {
  DCHECK_EQ(channel_, channel);
  channel_ = nil;
  [delegate_ resetOwner];
  delegate_ = nil;
  socket()->OnChannelClosed();
}

void BluetoothRfcommChannelMac::OnChannelDataReceived(
    IOBluetoothRFCOMMChannel* channel,
    void* data,
    size_t length) {
  DCHECK_EQ(channel_, channel);
  socket()->OnChannelDataReceived(data, length);
}

void BluetoothRfcommChannelMac::OnChannelWriteComplete(
    IOBluetoothRFCOMMChannel* channel,
    void* refcon,
    IOReturn status) {
  // Note: We use "CHECK" below to ensure we never run into unforeseen
  // occurrences of asynchronous callbacks, which could lead to data
  // corruption.
  CHECK_EQ(channel_, channel);
  socket()->OnChannelWriteComplete(refcon, status);
}

}  // namespace device
