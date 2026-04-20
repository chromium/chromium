// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_
#define REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/p2p_datagram_socket.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting::protocol {

// FakeDatagramSocket implement P2PStreamSocket interface. All data written to
// FakeDatagramSocket is stored in a buffer returned by written_packets().
// Read() reads data from another buffer that can be set with
// AppendInputPacket(). Pending reads are supported, so if there is a pending
// read AppendInputPacket() calls the read callback.
//
// Two fake sockets can be connected to each other using the
// PairWith() method, e.g.: a->PairWith(b). After this all data
// written to |a| can be read from |b| and vice versa. Two connected
// sockets |a| and |b| must be created and used on the same thread.
class FakeDatagramSocket : public P2PDatagramSocket {
 public:
  FakeDatagramSocket();

  FakeDatagramSocket(const FakeDatagramSocket&) = delete;
  FakeDatagramSocket& operator=(const FakeDatagramSocket&) = delete;

  ~FakeDatagramSocket() override;

  const std::vector<std::string>& written_packets() const {
    return written_packets_;
  }

  // Enables asynchronous Write().
  void set_async_send(bool async_send) { async_send_ = async_send; }

  // Set error codes for the next Write() call. Once returned the
  // value is automatically reset to net::OK .
  void set_next_send_error(net::Error error) { next_send_error_ = error; }

  void AppendInputPacket(const std::string& data);

  // Current position in the input in number of packets, i.e. number of finished
  // Recv() calls.
  int input_pos() const { return input_pos_; }

  // Pairs the socket with |peer_socket|. Deleting either of the paired sockets
  // unpairs them.
  void PairWith(FakeDatagramSocket* peer_socket);

  base::WeakPtr<FakeDatagramSocket> GetWeakPtr();

  // P2PDatagramSocket implementation.
  base::expected<base::ByteSize, net::Error> Recv(
      const scoped_refptr<net::IOBuffer>& buf,
      base::ByteSize buf_len,
      Callback callback) override;
  base::expected<base::ByteSize, net::Error> Send(
      const scoped_refptr<net::IOBuffer>& buf,
      base::ByteSize buf_len,
      Callback callback) override;

 private:
  base::ByteSize CopyReadData(const scoped_refptr<net::IOBuffer>& buf,
                              base::ByteSize buf_len);

  void DoAsyncSend(const scoped_refptr<net::IOBuffer>& buf,
                   base::ByteSize buf_len,
                   Callback callback);
  base::expected<base::ByteSize, net::Error> DoSend(
      const scoped_refptr<net::IOBuffer>& buf,
      base::ByteSize buf_len);

  bool async_send_ = false;
  bool send_pending_ = false;
  net::Error next_send_error_ = net::Error::OK;

  base::WeakPtr<FakeDatagramSocket> peer_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  base::ByteSize read_buffer_size_;
  P2PDatagramSocket::Callback read_callback_;

  std::vector<std::string> written_packets_;
  std::vector<std::string> input_packets_;
  int input_pos_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeDatagramSocket> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_DATAGRAM_SOCKET_H_
