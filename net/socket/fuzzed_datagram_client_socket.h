// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_FUZZED_DATAGRAM_CLIENT_SOCKET_H_
#define NET_SOCKET_FUZZED_DATAGRAM_CLIENT_SOCKET_H_

#include "base/memory/raw_ptr.h"
#include "net/socket/datagram_client_socket.h"

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class FuzzedDataProvider;

namespace net {

class IOBuffer;

// Datagram ClientSocket implementation for use with fuzzers. Can fail to
// connect, reads and writes can succeed or fail synchronously or
// asynchronously.  Successful reads return random data.
class FuzzedDatagramClientSocket : public DatagramClientSocket {
 public:
  // |data_provider| must outlive the created socket.
  explicit FuzzedDatagramClientSocket(FuzzedDataProvider* data_provider);

  FuzzedDatagramClientSocket(const FuzzedDatagramClientSocket&) = delete;
  FuzzedDatagramClientSocket& operator=(const FuzzedDatagramClientSocket&) =
      delete;

  ~FuzzedDatagramClientSocket() override;

  // DatagramClientSocket implementation:
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) override;
  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  DscpAndEcn GetLastTos() const override;

  // DatagramSocket implementation:
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  int SetMulticastInterface(uint32_t interface_index) override;

  const NetLogWithSource& NetLog() const override;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override;
  void SetMsgConfirm(bool confirm) override {}

 private:
  void OnReadComplete(net::CompletionOnceCallback callback, int result);
  void OnWriteComplete(net::CompletionOnceCallback callback, int result);

  raw_ptr<FuzzedDataProvider> data_provider_;

  bool connected_ = false;
  bool read_pending_ = false;
  bool write_pending_ = false;

  NetLogWithSource net_log_;

  IPEndPoint remote_address_;

  base::WeakPtrFactory<FuzzedDatagramClientSocket> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_FUZZED_DATAGRAM_CLIENT_SOCKET_H_
