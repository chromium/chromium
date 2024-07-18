// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>

#include "base/logging.h"
#include "base/run_loop.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/session_usage.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

const uint8_t kCertData[] = {
#include "net/data/ssl/certificates/spdy_pooling.inc"
};

class FuzzerDelegate : public net::SpdyStream::Delegate {
 public:
  explicit FuzzerDelegate(base::OnceClosure done_closure)
      : done_closure_(std::move(done_closure)) {}

  FuzzerDelegate(const FuzzerDelegate&) = delete;
  FuzzerDelegate& operator=(const FuzzerDelegate&) = delete;

  void OnHeadersSent() override {}
  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override {}
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {}
  void OnDataReceived(std::unique_ptr<net::SpdyBuffer> buffer) override {}
  void OnDataSent() override {}
  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override {}
  void OnClose(int status) override { std::move(done_closure_).Run(); }
  bool CanGreaseFrameType() const override { return false; }

  net::NetLogSource source_dependency() const override {
    return net::NetLogSource();
  }

 private:
  base::OnceClosure done_closure_;
};

}  // namespace

namespace net {

namespace {

class FuzzedSocketFactoryWithMockSSLData : public FuzzedSocketFactory {
 public:
  explicit FuzzedSocketFactoryWithMockSSLData(
      FuzzedDataProvider* data_provider);

  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> nested_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override;

 private:
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;
};

FuzzedSocketFactoryWithMockSSLData::FuzzedSocketFactoryWithMockSSLData(
    FuzzedDataProvider* data_provider)
    : FuzzedSocketFactory(data_provider) {}

void FuzzedSocketFactoryWithMockSSLData::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

std::unique_ptr<SSLClientSocket>
FuzzedSocketFactoryWithMockSSLData::CreateSSLClientSocket(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> nested_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  return std::make_unique<MockSSLClientSocket>(std::move(nested_socket),
                                               host_and_port, ssl_config,
                                               mock_ssl_data_.GetNext());
}

}  // namespace

}  // namespace net

// Fuzzer for SpdySession
//
// |data| is used to create a FuzzedServerSocket.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Including an observer; even though the recorded results aren't currently
  // used, it'll ensure the netlogging code is fuzzed as well.
  net::RecordingNetLogObserver net_log_observer;
  net::NetLogWithSource net_log_with_source =
      net::NetLogWithSource::Make(net::NetLogSourceType::NONE);
  FuzzedDataProvider data_provider(data, size);
  net::FuzzedSocketFactoryWithMockSSLData socket_factory(&data_provider);
  socket_factory.set_fuzz_connect_result(false);

  net::SSLSocketDataProvider ssl_provider(net::ASYNC, net::OK);
  ssl_provider.ssl_info.cert = net::X509Certificate::CreateFromBytes(kCertData);
  CHECK(ssl_provider.ssl_info.cert);
  socket_factory.AddSSLSocketDataProvider(&ssl_provider);

  net::SpdySessionDependencies deps;
  std::unique_ptr<net::HttpNetworkSession> http_session(
      net::SpdySessionDependencies::SpdyCreateSessionWithSocketFactory(
          &deps, &socket_factory));

  net::SpdySessionKey session_key(
      net::HostPortPair("127.0.0.1", 80), net::PRIVACY_MODE_DISABLED,
      net::ProxyChain::Direct(), net::SessionUsage::kDestination,
      net::SocketTag(), net::NetworkAnonymizationKey(),
      net::SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false);
  base::WeakPtr<net::SpdySession> spdy_session(net::CreateSpdySession(
      http_session.get(), session_key, net_log_with_source));

  net::SpdyStreamRequest stream_request;
  base::WeakPtr<net::SpdyStream> stream;

  net::TestCompletionCallback wait_for_start;
  int rv = stream_request.StartRequest(
      net::SPDY_REQUEST_RESPONSE_STREAM, spdy_session,
      GURL("http://www.example.invalid/"), /*can_send_early=*/false,
      net::DEFAULT_PRIORITY, net::SocketTag(), net_log_with_source,
      wait_for_start.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);

  if (rv == net::ERR_IO_PENDING) {
    rv = wait_for_start.WaitForResult();
  }

  // Re-check the status after potential event loop.
  if (rv != net::OK) {
    LOG(WARNING) << "StartRequest failed with result=" << rv;
    return 0;
  }

  stream = stream_request.ReleaseStream();
  stream->SendRequestHeaders(
      net::SpdyTestUtil::ConstructGetHeaderBlock("http://www.example.invalid"),
      net::NO_MORE_DATA_TO_SEND);

  base::RunLoop run_loop;
  FuzzerDelegate delegate(run_loop.QuitClosure());
  stream->SetDelegate(&delegate);
  run_loop.Run();

  // Give a chance for GOING_AWAY sessions to wrap up.
  base::RunLoop().RunUntilIdle();

  return 0;
}
