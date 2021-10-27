// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_
#define REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/webrtc/api/packet_socket_factory.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;

}  // namespace net

namespace remoting {
namespace protocol {

class StreamPacketProcessor;

// An AsyncPacketSocket implementation that runs on top of a StreamSocket. It is
// usually used for TCP connections.
// TODO(yuweih): Write unittest
class StreamPacketSocket final : public rtc::AsyncPacketSocket {
 public:
  StreamPacketSocket();
  ~StreamPacketSocket() override;

  StreamPacketSocket(const StreamPacketSocket&) = delete;
  StreamPacketSocket& operator=(const StreamPacketSocket&) = delete;

  // Initializes the packet socket with the underlying stream socket and packet
  // processor. Returns true if the initialization succeeds.
  bool Init(std::unique_ptr<net::StreamSocket> socket,
            StreamPacketProcessor* packet_processor);

  // Initializes the packet socket for client TCP connection. Returns true if
  // the initialization succeeds.
  bool InitClientTcp(const rtc::SocketAddress& local_address,
                     const rtc::SocketAddress& remote_address,
                     const rtc::ProxyInfo& proxy_info,
                     const std::string& user_agent,
                     const rtc::PacketSocketTcpOptions& tcp_options);

  // rtc::AsyncPacketSocket interface.
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Send(const void* data,
           size_t data_size,
           const rtc::PacketOptions& options) override;
  int SendTo(const void* data,
             size_t data_size,
             const rtc::SocketAddress& address,
             const rtc::PacketOptions& options) override;
  int Close() override;
  State GetState() const override;
  int GetOption(rtc::Socket::Option option, int* value) override;
  int SetOption(rtc::Socket::Option option, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 private:
  struct PendingPacket {
    PendingPacket(scoped_refptr<net::DrainableIOBuffer> data,
                  rtc::PacketOptions options);
    PendingPacket(const PendingPacket&);
    PendingPacket(PendingPacket&&);
    ~PendingPacket();

    scoped_refptr<net::DrainableIOBuffer> data;
    rtc::PacketOptions options;
  };

  void OnConnectCompleted(int result);

  void DoWrite();
  bool HandleWriteResult(int result);
  void OnAsyncWriteCompleted(int result);

  void DoRead();
  bool HandleReadResult(int result);
  void OnAsyncReadCompleted(int result);

  // Translates net_error to errno and set it on error_, then closes the socket
  // and reports the error.
  // If you only need to set the error code (in errno), call SetError instead.
  void CloseWithNetError(int net_error);

  std::unique_ptr<net::StreamSocket> socket_;
  StreamPacketProcessor* packet_processor_;

  // Note that a packet can be partially sent, where the number of bytes sent
  // is reflected in DrainableIOBuffer::BytesConsumed.
  base::circular_deque<PendingPacket> send_queue_;

  bool send_pending_ = false;

  // The offset denotes the position for new data to arrive, whereas data
  // between [0, offset) are arrived but not processed.
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  State state_ = STATE_CLOSED;

  // This is errno, not the net error code, which is what rtc::AsyncPacketSocket
  // accepts.
  // Unlike //net classes, rtc::AsyncPacketSocket methods don't return the error
  // code. For error, they generally return -1 and expect the caller to call
  // GetError() to know the reason.
  int error_ = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_
