// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_
#define REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/webrtc/api/packet_socket_factory.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;

}  // namespace net

namespace remoting::protocol {

class StreamPacketProcessor;

// An AsyncPacketSocket implementation that runs on top of a StreamSocket. It is
// usually used for TCP connections.
class StreamPacketSocket final : public webrtc::AsyncPacketSocket {
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
  bool InitClientTcp(const webrtc::SocketAddress& local_address,
                     const webrtc::SocketAddress& remote_address,
                     const webrtc::PacketSocketTcpOptions& tcp_options);

  // webrtc::AsyncPacketSocket interface.
  webrtc::SocketAddress GetLocalAddress() const override;
  webrtc::SocketAddress GetRemoteAddress() const override;
  int Send(const void* data,
           size_t data_size,
           const webrtc::AsyncSocketPacketOptions& options) override;
  int SendTo(const void* data,
             size_t data_size,
             const webrtc::SocketAddress& address,
             const webrtc::AsyncSocketPacketOptions& options) override;
  int Close() override;
  State GetState() const override;
  int GetOption(webrtc::Socket::Option option, int* value) override;
  int SetOption(webrtc::Socket::Option option, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 private:
  struct PendingPacket {
    PendingPacket(scoped_refptr<net::DrainableIOBuffer> data,
                  webrtc::AsyncSocketPacketOptions options);
    PendingPacket(const PendingPacket&);
    PendingPacket(PendingPacket&&);
    ~PendingPacket();

    scoped_refptr<net::DrainableIOBuffer> data;
    webrtc::AsyncSocketPacketOptions options;
  };

  void OnConnectCompleted(int result);

  void DoWrite();
  bool HandleWriteResult(int result);
  void OnAsyncWriteCompleted(int result);

  void DoRead();
  bool HandleReadResult(int result);
  void OnAsyncReadCompleted(int result);

  // Translates net_error to errno and sets it on error_, then closes the socket
  // and posts an asynchronous close notification.
  // If you only need to set the error code (in errno), call SetError instead.
  void CloseWithNetError(int net_error);

  // Invoked via a posted task from CloseWithNetError() to notify listeners
  // after the current call stack unwinds. Cancelled by Close() or destruction.
  void RunNotifyClosed(int error);

  raw_ptr<StreamPacketProcessor> packet_processor_;

  // Note that a packet can be partially sent, where the number of bytes sent
  // is reflected in DrainableIOBuffer::BytesConsumed.
  base::circular_deque<PendingPacket> send_queue_;

  bool send_pending_ = false;
  bool in_do_write_ = false;

  // The offset denotes the position for new data to arrive, whereas data
  // between [0, offset) are arrived but not processed.
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  State state_ = STATE_CLOSED;

  // This is errno, not the net error code, which is what
  // webrtc::AsyncPacketSocket accepts. Unlike //net classes,
  // webrtc::AsyncPacketSocket methods don't return the error code. For error,
  // they generally return -1 and expect the caller to call GetError() to know
  // the reason.
  int error_ = 0;

  // Must be declared after all members referenced by pending I/O callbacks
  // (bound with base::Unretained(this)) so it is destroyed first, cancelling
  // those callbacks before the state and buffers they use are released.
  std::unique_ptr<net::StreamSocket> socket_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Invalidated in Close() to cancel pending RunNotifyClosed() tasks while
  // keeping `weak_factory_` valid across Close() so DoWrite() and
  // OnAsyncWriteCompleted() still reset `in_do_write_` unless `this` was
  // destroyed.
  base::WeakPtrFactory<StreamPacketSocket> close_weak_factory_{this};
  base::WeakPtrFactory<StreamPacketSocket> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_STREAM_PACKET_SOCKET_H_
