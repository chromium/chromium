// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_FAKE_SSL_CLIENT_SOCKET_H_
#define NET_SOCKET_FAKE_SSL_CLIENT_SOCKET_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_info.h"

namespace net {

class X509Certificate;

// A fake SSL client socket that wraps a regular stream socket and pretends
// to be a secure SSL connection. Used for .dapp domains which connect to
// local servers that don't have real SSL certificates.
class NET_EXPORT FakeSSLClientSocket : public SSLClientSocket {
 public:
  FakeSSLClientSocket(std::unique_ptr<StreamSocket> stream_socket,
                      const HostPortPair& host_and_port);
  ~FakeSSLClientSocket() override;

  // StreamSocket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  std::optional<std::string_view> GetPeerApplicationSettings() const override;
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) const override;

  // SSLSocket implementation.
  int ExportKeyingMaterial(std::string_view label,
                           std::optional<base::span<const uint8_t>> context,
                           base::span<uint8_t> out) override;

  // SSLClientSocket implementation.
  std::vector<uint8_t> GetECHRetryConfigs() override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  std::vector<std::vector<uint8_t>> GetServerTrustAnchorIDsForRetry() override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

 private:
  std::unique_ptr<StreamSocket> stream_socket_;
  HostPortPair host_and_port_;
  bool is_connected_;
  scoped_refptr<X509Certificate> fake_cert_;

  void CreateFakeCertificate();
};

}  // namespace net

#endif  // NET_SOCKET_FAKE_SSL_CLIENT_SOCKET_H_