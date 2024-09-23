// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CLIENT_SOCKET_H_
#define NET_SOCKET_SOCKS_CLIENT_SOCKET_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// The SOCKS client socket implementation
class NET_EXPORT_PRIVATE SOCKSClientSocket : public StreamSocket {
 public:
  // |destination| contains the hostname and port to which the socket above will
  // communicate to via the socks layer. For testing the referrer is optional.
  // |network_anonymization_key| is used for host resolution.
  SOCKSClientSocket(std::unique_ptr<StreamSocket> transport_socket,
                    const HostPortPair& destination,
                    const NetworkAnonymizationKey& network_anonymization_key,
                    RequestPriority priority,
                    HostResolver* host_resolver,
                    SecureDnsPolicy secure_dns_policy,
                    const NetworkTrafficAnnotationTag& traffic_annotation);

  SOCKSClientSocket(const SOCKSClientSocket&) = delete;
  SOCKSClientSocket& operator=(const SOCKSClientSocket&) = delete;

  // On destruction Disconnect() is called.
  ~SOCKSClientSocket() override;

  // StreamSocket implementation.

  // Does the SOCKS handshake and completes the protocol.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;

  // Returns error information about any host resolution attempt.
  ResolveErrorInfo GetResolveErrorInfo() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, CompleteHandshake);
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, SOCKS4AFailedDNS);
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, SOCKS4AIfDomainInIPv6);

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_NONE,
  };

  void DoCallback(int result);
  void OnIOComplete(int result);
  void OnReadWriteComplete(CompletionOnceCallback callback, int result);

  int DoLoop(int last_io_result);
  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);

  const std::string BuildHandshakeWriteBuffer() const;

  // Stores the underlying socket.
  std::unique_ptr<StreamSocket> transport_socket_;

  State next_state_ = STATE_NONE;

  // Stores the callbacks to the layer above, called on completing Connect().
  CompletionOnceCallback user_callback_;

  // This IOBuffer is used by the class to read and write
  // SOCKS handshake data. The length contains the expected size to
  // read or write.
  scoped_refptr<IOBuffer> handshake_buf_;

  // While writing, this buffer stores the complete write handshake data.
  // While reading, it stores the handshake information received so far.
  std::string buffer_;

  // This becomes true when the SOCKS handshake has completed and the
  // overlying connection is free to communicate.
  bool completed_handshake_ = false;

  // These contain the bytes sent / received by the SOCKS handshake.
  size_t bytes_sent_ = 0;
  size_t bytes_received_ = 0;

  // This becomes true when the socket is used to send or receive data.
  bool was_ever_used_ = false;

  // Used to resolve the hostname to which the SOCKS proxy will connect.
  raw_ptr<HostResolver> host_resolver_;
  SecureDnsPolicy secure_dns_policy_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;
  const HostPortPair destination_;
  const NetworkAnonymizationKey network_anonymization_key_;
  RequestPriority priority_;
  ResolveErrorInfo resolve_error_info_;

  NetLogWithSource net_log_;

  // Traffic annotation for socket control.
  NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CLIENT_SOCKET_H_
