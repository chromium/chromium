// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/fuzzed_socket_factory.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <string_view>

#include "base/notreached.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/fuzzed_datagram_client_socket.h"
#include "net/socket/fuzzed_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLog;

namespace {

// SSLClientSocket implementation that always fails to connect.
class FailingSSLClientSocket : public SSLClientSocket {
 public:
  FailingSSLClientSocket() = default;

  FailingSSLClientSocket(const FailingSSLClientSocket&) = delete;
  FailingSSLClientSocket& operator=(const FailingSSLClientSocket&) = delete;

  ~FailingSSLClientSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    NOTREACHED_IN_MIGRATION();
    return ERR_UNEXPECTED;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTREACHED_IN_MIGRATION();
    return ERR_UNEXPECTED;
  }

  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

  // StreamSocket implementation:
  int Connect(CompletionOnceCallback callback) override { return ERR_FAILED; }

  void Disconnect() override {}
  bool IsConnected() const override { return false; }
  bool IsConnectedAndIdle() const override { return false; }

  int GetPeerAddress(IPEndPoint* address) const override {
    return ERR_SOCKET_NOT_CONNECTED;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  const NetLogWithSource& NetLog() const override { return net_log_; }

  bool WasEverUsed() const override { return false; }

  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }

  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }

  int64_t GetTotalReceivedBytes() const override { return 0; }

  void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) const override {}

  void ApplySocketTag(const net::SocketTag& tag) override {}

  // SSLSocket implementation:
  int ExportKeyingMaterial(std::string_view label,
                           bool has_context,
                           std::string_view context,
                           unsigned char* out,
                           unsigned int outlen) override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  // SSLClientSocket implementation:
  std::vector<uint8_t> GetECHRetryConfigs() override {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

 private:
  NetLogWithSource net_log_;
};

}  // namespace

FuzzedSocketFactory::FuzzedSocketFactory(FuzzedDataProvider* data_provider)
    : data_provider_(data_provider) {}

FuzzedSocketFactory::~FuzzedSocketFactory() = default;

std::unique_ptr<DatagramClientSocket>
FuzzedSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    NetLog* net_log,
    const NetLogSource& source) {
  return std::make_unique<FuzzedDatagramClientSocket>(data_provider_);
}

std::unique_ptr<TransportClientSocket>
FuzzedSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    const NetLogSource& source) {
  auto socket = std::make_unique<FuzzedSocket>(data_provider_, net_log);
  socket->set_fuzz_connect_result(fuzz_connect_result_);
  // Just use the first address.
  socket->set_remote_address(*addresses.begin());
  return std::move(socket);
}

std::unique_ptr<SSLClientSocket> FuzzedSocketFactory::CreateSSLClientSocket(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  return std::make_unique<FailingSSLClientSocket>();
}

}  // namespace net
