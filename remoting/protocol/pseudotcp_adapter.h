// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PSEUDOTCP_ADAPTER_H_
#define REMOTING_PROTOCOL_PSEUDOTCP_ADAPTER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "third_party/webrtc/p2p/base/pseudo_tcp.h"

namespace remoting::protocol {

class P2PDatagramSocket;

// PseudoTcpAdapter adapts a P2PDatagramSocket to P2PStreamSocket using
// PseudoTcp. Because P2PStreamSockets can be deleted during callbacks,
// while PseudoTcp cannot, the core of the PseudoTcpAdapter is reference
// counted, with a reference held by the adapter, and an additional reference
// held on the stack during callbacks.
class PseudoTcpAdapter : public P2PStreamSocket {
 public:
  explicit PseudoTcpAdapter(std::unique_ptr<P2PDatagramSocket> socket);

  PseudoTcpAdapter(const PseudoTcpAdapter&) = delete;
  PseudoTcpAdapter& operator=(const PseudoTcpAdapter&) = delete;

  ~PseudoTcpAdapter() override;

  // P2PStreamSocket implementation.
  int Read(const scoped_refptr<net::IOBuffer>& buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override;
  int Write(
      const scoped_refptr<net::IOBuffer>& buffer,
      int buffer_size,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  // If the connection succeeds, this will take ownership of |callback| and
  // return a null callback. If it fails, |callback| will be passed back to the
  // caller.
  net::CompletionOnceCallback Connect(net::CompletionOnceCallback callback);

  // Set receive and send buffer sizes.
  int SetReceiveBufferSize(int32_t size);
  int SetSendBufferSize(int32_t size);

  // Set the delay for sending ACK.
  void SetAckDelay(int delay_ms);

  // Set whether Nagle's algorithm is enabled.
  void SetNoDelay(bool no_delay);

  // When write_waits_for_send flag is set to true the Write() method
  // will wait until the data is sent to the remote end before the
  // write completes (it still doesn't wait until the data is received
  // and acknowledged by the remote end). Otherwise write completes
  // after the data has been copied to the send buffer.
  //
  // This flag is useful in cases when the sender needs to get
  // feedback from the connection when it is congested. E.g. remoting
  // host uses this feature to adjust screen capturing rate according
  // to the available bandwidth. In the same time it may negatively
  // impact performance in some cases. E.g. when the sender writes one
  // byte at a time then each byte will always be sent in a separate
  // packet.
  //
  // TODO(sergeyu): Remove this flag once remoting has a better
  // flow-control solution.
  void SetWriteWaitsForSend(bool write_waits_for_send);

 private:
  class Core;

  scoped_refptr<Core> core_;

  net::NetLogWithSource net_log_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PSEUDOTCP_ADAPTER_H_
