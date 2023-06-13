// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_rfcomm_channel_mac.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A simple delegate class for an open RFCOMM channel that forwards methods to
// its wrapped |channel_|.
@interface BluetoothRfcommChannelDelegate
    : NSObject <IOBluetoothRFCOMMChannelDelegate> {
 @private
  raw_ptr<device::BluetoothRfcommChannelMac> _channel;  // weak
}

- (instancetype)initWithChannel:(device::BluetoothRfcommChannelMac*)channel;

@end

@implementation BluetoothRfcommChannelDelegate

- (instancetype)initWithChannel:(device::BluetoothRfcommChannelMac*)channel {
  if ((self = [super init]))
    _channel = channel;

  return self;
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                           status:(IOReturn)error {
  _channel->OnChannelOpenComplete(rfcommChannel, error);
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
                            refcon:(void*)refcon
                            status:(IOReturn)error {
  _channel->OnChannelWriteComplete(rfcommChannel, refcon, error);
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  _channel->OnChannelDataReceived(rfcommChannel, dataPointer, dataLength);
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  _channel->OnChannelClosed(rfcommChannel);
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
  [channel_ setDelegate:nil];
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
  delegate_ = [[BluetoothRfcommChannelDelegate alloc] initWithChannel:this];
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
