// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/dedicated_web_transport_http3_client.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/quic_context.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_backend.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/tools/quic/quic_simple_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::test {
namespace {

using ::quic::test::MemSliceFromString;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Optional;
using ::testing::SaveArg;

class MockVisitor : public WebTransportClientVisitor {
 public:
  MOCK_METHOD(void,
              OnConnected,
              (scoped_refptr<HttpResponseHeaders>),
              (override));
  MOCK_METHOD(void, OnConnectionFailed, (const WebTransportError&), (override));
  MOCK_METHOD(void,
              OnClosed,
              (const std::optional<WebTransportCloseInfo>&),
              (override));
  MOCK_METHOD(void, OnError, (const WebTransportError&), (override));

  MOCK_METHOD0(OnIncomingBidirectionalStreamAvailable, void());
  MOCK_METHOD0(OnIncomingUnidirectionalStreamAvailable, void());
  MOCK_METHOD1(OnDatagramReceived, void(std::string_view));
  MOCK_METHOD0(OnCanCreateNewOutgoingBidirectionalStream, void());
  MOCK_METHOD0(OnCanCreateNewOutgoingUnidirectionalStream, void());
  MOCK_METHOD1(OnDatagramProcessed, void(std::optional<quic::MessageStatus>));
};

// A clock that only mocks out WallNow(), but uses real Now() and
// ApproximateNow().  Useful for certificate verification.
class TestWallClock : public quic::QuicClock {
 public:
  quic::QuicTime Now() const override {
    return quic::QuicChromiumClock::GetInstance()->Now();
  }
  quic::QuicTime ApproximateNow() const override {
    return quic::QuicChromiumClock::GetInstance()->ApproximateNow();
  }
  quic::QuicWallTime WallNow() const override { return wall_now_; }

  void set_wall_now(quic::QuicWallTime now) { wall_now_ = now; }

 private:
  quic::QuicWallTime wall_now_ = quic::QuicWallTime::Zero();
};

class TestConnectionHelper : public quic::QuicConnectionHelperInterface {
 public:
  const quic::QuicClock* GetClock() const override { return &clock_; }
  quic::QuicRandom* GetRandomGenerator() override {
    return quic::QuicRandom::GetInstance();
  }
  quiche::QuicheBufferAllocator* GetStreamSendBufferAllocator() override {
    return &allocator_;
  }

  TestWallClock& clock() { return clock_; }

 private:
  TestWallClock clock_;
  quiche::SimpleBufferAllocator allocator_;
};

class DedicatedWebTransportHttp3Test : public TestWithTaskEnvironment {
 public:
  ~DedicatedWebTransportHttp3Test() override {
    if (server_ != nullptr) {
      server_->Shutdown();
    }
  }

  void SetUp() override {
    BuildContext(ConfiguredProxyResolutionService::CreateDirect());
    quic::QuicEnableVersion(quic::ParsedQuicVersion::RFCv1());
    origin_ = url::Origin::Create(GURL{"https://example.org"});
    anonymization_key_ =
        NetworkAnonymizationKey::CreateSameSite(SchemefulSite(origin_));

    // By default, quit on error instead of waiting for RunLoop() to time out.
    ON_CALL(visitor_, OnConnectionFailed(_))
        .WillByDefault([this](const WebTransportError& error) {
          LOG(ERROR) << "Connection failed: " << error;
          if (run_loop_) {
            run_loop_->Quit();
          }
        });
    ON_CALL(visitor_, OnError(_))
        .WillByDefault([this](const WebTransportError& error) {
          LOG(ERROR) << "Connection error: " << error;
          if (run_loop_) {
            run_loop_->Quit();
          }
        });
  }

  // Use a URLRequestContextBuilder to set `context_`.
  void BuildContext(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    URLRequestContextBuilder builder;
    builder.set_proxy_resolution_service(std::move(proxy_resolution_service));

    auto cert_verifier = std::make_unique<MockCertVerifier>();
    cert_verifier->set_default_result(OK);
    builder.SetCertVerifier(std::move(cert_verifier));

    auto host_resolver = std::make_unique<MockHostResolver>();
    host_resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    builder.set_host_resolver(std::move(host_resolver));

    auto helper = std::make_unique<TestConnectionHelper>();
    helper_ = helper.get();
    auto quic_context = std::make_unique<QuicContext>(std::move(helper));
    quic_context->params()->supported_versions.clear();
    // This is required to bypass the check that only allows known certificate
    // roots in QUIC.
    quic_context->params()->origins_to_force_quic_on.insert(
        HostPortPair("test.example.com", 0));
    builder.set_quic_context(std::move(quic_context));

    builder.set_net_log(NetLog::Get());
    context_ = builder.Build();
  }

  GURL GetURL(const std::string& suffix) {
    return GURL{base::StrCat(
        {"https://test.example.com:", base::NumberToString(port_), suffix})};
  }

  void StartServer(std::unique_ptr<quic::ProofSource> proof_source = nullptr) {
    if (proof_source == nullptr) {
      proof_source = quic::test::crypto_test_utils::ProofSourceForTesting();
    }
    backend_.set_enable_webtransport(true);
    server_ = std::make_unique<QuicSimpleServer>(
        std::move(proof_source), quic::QuicConfig(),
        quic::QuicCryptoServerConfig::ConfigOptions(),
        AllSupportedQuicVersions(), &backend_);
    ASSERT_TRUE(server_->CreateUDPSocketAndListen(
        quic::QuicSocketAddress(quic::QuicIpAddress::Any6(), /*port=*/0)));
    port_ = server_->server_address().port();
  }

  void Run() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  auto StopRunning() {
    return [this]() {
      if (run_loop_) {
        run_loop_->Quit();
      }
    };
  }

 protected:
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<DedicatedWebTransportHttp3Client> client_;
  raw_ptr<TestConnectionHelper> helper_;  // Owned by |context_|.
  ::testing::NiceMock<MockVisitor> visitor_;
  std::unique_ptr<QuicSimpleServer> server_;
  std::unique_ptr<base::RunLoop> run_loop_;
  quic::test::QuicTestBackend backend_;

  int port_ = 0;
  url::Origin origin_;
  NetworkAnonymizationKey anonymization_key_;
};

TEST_F(DedicatedWebTransportHttp3Test, Connect) {
  StartServer();
  client_ = std::make_unique<DedicatedWebTransportHttp3Client>(
      GetURL("/echo"), origin_, &visitor_, anonymization_key_, context_.get(),
      WebTransportParameters());

  EXPECT_CALL(visitor_, OnConnected(_)).WillOnce(StopRunning());
  client_->Connect();
  Run();
  ASSERT_TRUE(client_->session() != nullptr);

  client_->Close(std::nullopt);
  EXPECT_CALL(visitor_, OnClosed(_)).WillOnce(StopRunning());
  Run();
}

// Check that connecting via a proxy fails. This is currently not implemented,
// but it's important that WebTransport not be usable to _bypass_ a proxy -- if
// a proxy is configured, it must be used.
TEST_F(DedicatedWebTransportHttp3Test, ConnectViaProxy) {
  BuildContext(
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "test",
                                             80)},
          TRAFFIC_ANNOTATION_FOR_TESTS));
  StartServer();
  client_ = std::make_unique<DedicatedWebTransportHttp3Client>(
      GetURL("/echo"), origin_, &visitor_, anonymization_key_, context_.get(),
      WebTransportParameters());

  // This will fail before the run loop starts.
  EXPECT_CALL(visitor_, OnConnectionFailed(_));
  client_->Connect();
}

// TODO(crbug.com/40816637): The test is flaky on Mac and iOS.
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CloseTimeout DISABLED_CloseTimeout
#else
#define MAYBE_CloseTimeout CloseTimeout
#endif
TEST_F(DedicatedWebTransportHttp3Test, MAYBE_CloseTimeout) {
  StartServer();
  client_ = std::make_unique<DedicatedWebTransportHttp3Client>(
      GetURL("/echo"), origin_, &visitor_, anonymization_key_, context_.get(),
      WebTransportParameters());

  EXPECT_CALL(visitor_, OnConnected(_)).WillOnce(StopRunning());
  client_->Connect();
  Run();
  ASSERT_TRUE(client_->session() != nullptr);

  // Delete the server and put up a no-op socket in its place to simulate the
  // traffic being dropped.  Note that this is normally not a supported way of
  // shutting down a QuicServer, and will generate a lot of errors in the logs.
  server_.reset();
  IPEndPoint bind_address(IPAddress::IPv6AllZeros(), port_);
  auto noop_socket =
      std::make_unique<UDPServerSocket>(/*net_log=*/nullptr, NetLogSource());
  noop_socket->AllowAddressReuse();
  ASSERT_GE(noop_socket->Listen(bind_address), 0);

  client_->Close(std::nullopt);
  EXPECT_CALL(visitor_, OnError(_)).WillOnce(StopRunning());
  Run();
}

TEST_F(DedicatedWebTransportHttp3Test, CloseReason) {
  StartServer();
  client_ = std::make_unique<DedicatedWebTransportHttp3Client>(
      GetURL("/session-close"), origin_, &visitor_, anonymization_key_,
      context_.get(), WebTransportParameters());

  EXPECT_CALL(visitor_, OnConnected(_)).WillOnce(StopRunning());
  client_->Connect();
  Run();
  ASSERT_TRUE(client_->session() != nullptr);

  quic::WebTransportStream* stream =
      client_->session()->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_TRUE(stream->Write("42 test error"));
  EXPECT_TRUE(stream->SendFin());

  WebTransportCloseInfo close_info(42, "test error");
  std::optional<WebTransportCloseInfo> received_close_info;
  EXPECT_CALL(visitor_, OnClosed(_))
      .WillOnce(DoAll(StopRunning(), SaveArg<0>(&received_close_info)));
  Run();
  EXPECT_THAT(received_close_info, Optional(close_info));
}

}  // namespace
}  // namespace net::test
