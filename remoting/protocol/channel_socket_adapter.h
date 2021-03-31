// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_
#define REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/p2p_datagram_socket.h"
// TODO(zhihuang):Replace #include by forward declaration once proper
// inheritance is defined for cricket::IceTransportInternal and
// cricket::P2PTransportChannel.
#include "third_party/webrtc/p2p/base/ice_transport_internal.h"
// TODO(johan): Replace #include by forward declaration once proper
// inheritance is defined for rtc::PacketTransportInterface and
// cricket::TransportChannel.
#include "third_party/webrtc/p2p/base/packet_transport_internal.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc/rtc_base/third_party/sigslot/sigslot.h"

namespace remoting {
namespace protocol {

// TransportChannelSocketAdapter implements P2PDatagramSocket interface on
// top of cricket::IceTransportInternal. It is used by IceTransport to provide
// P2PDatagramSocket interface for channels.
class TransportChannelSocketAdapter : public P2PDatagramSocket,
                                      public sigslot::has_slots<> {
 public:
  // Doesn't take ownership of the |channel|. The |channel| must outlive
  // this adapter.
  explicit TransportChannelSocketAdapter(
      cricket::IceTransportInternal* ice_transport);
  ~TransportChannelSocketAdapter() override;

  // Sets callback that should be called when the adapter is being
  // destroyed. The callback is not allowed to touch the adapter, but
  // can do anything else, e.g. destroy the TransportChannel.
  void SetOnDestroyedCallback(base::OnceClosure callback);

  // Closes the stream. |error_code| specifies error code that will
  // be returned by Recv() and Send() after the stream is closed.
  // Must be called before the session and the channel are destroyed.
  void Close(int error_code);

  // P2PDatagramSocket interface.
  int Recv(const scoped_refptr<net::IOBuffer>& buf,
           int buf_len,
           const net::CompletionRepeatingCallback& callback) override;
  int Send(const scoped_refptr<net::IOBuffer>& buf,
           int buf_len,
           const net::CompletionRepeatingCallback& callback) override;

 private:
  void OnNewPacket(rtc::PacketTransportInternal* transport,
                   const char* data,
                   size_t data_size,
                   const int64_t& packet_time,
                   int flags);
  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnChannelDestroyed(cricket::IceTransportInternal* ice_transport);

  base::ThreadChecker thread_checker_;

  cricket::IceTransportInternal* channel_;

  base::OnceClosure destruction_callback_;

  net::CompletionRepeatingCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  net::CompletionRepeatingCallback write_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  int closed_error_code_;

  DISALLOW_COPY_AND_ASSIGN(TransportChannelSocketAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_SOCKET_ADAPTER_H_
