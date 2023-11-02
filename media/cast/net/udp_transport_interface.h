// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_UDP_TRANSPORT_INTERFACE_H_
#define MEDIA_CAST_NET_UDP_TRANSPORT_INTERFACE_H_

#include "mojo/public/cpp/system/data_pipe.h"

namespace media {
namespace cast {

// TODO(xjz): Define these interfaces in the mojom file.

// This class receives packets from the UdpTransportHost.
class UdpTransportReceiver {
 public:
  virtual ~UdpTransportReceiver() {}
  // Called when a UDP packet is received.
  virtual void OnPacketReceived(const std::vector<uint8_t>& packet) = 0;
};

// When requested to start receiving packets, this class receives UDP packets
// and passes them to the UdpTransportReceiver.
// When requested to start sending packets, this class reads UDP packets from
// the mojo data pipe and sends them over network.
class UdpTransport {
 public:
  virtual ~UdpTransport() {}
  // Called to start/stop receiving UDP packets. The received UDP packets will
  // be passed to the |receiver|.
  // TODO(xjz): Use a data pipe for received packets if this interface starts
  // being used by cast receiver in future. For now, this is only used by cast
  // sender, which only receives small-sized RTCP packets.
  virtual void StartReceiving(UdpTransportReceiver* receiver) = 0;
  virtual void StopReceiving() = 0;
  // Called to start sending the UDP packets that received through the mojo data
  // pipe.
  virtual void StartSending(mojo::ScopedDataPipeConsumerHandle packet_pipe) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_UDP_TRANSPORT_INTERFACE_H_
