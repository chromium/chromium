// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_transport_client.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/schemeful_site.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace test {
namespace {

using ::quic::test::MemSliceFromString;
using ::testing::_;

class MockVisitor : public QuicTransportClient::Visitor {
 public:
  MOCK_METHOD0(OnConnected, void());
  MOCK_METHOD0(OnConnectionFailed, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());

  MOCK_METHOD0(OnIncomingBidirectionalStreamAvailable, void());
  MOCK_METHOD0(OnIncomingUnidirectionalStreamAvailable, void());
  MOCK_METHOD1(OnDatagramReceived, void(base::StringPiece));
  MOCK_METHOD0(OnCanCreateNewOutgoingBidirectionalStream, void());
  MOCK_METHOD0(OnCanCreateNewOutgoingUnidirectionalStream, void());
  MOCK_METHOD1(OnDatagramProcessed, void(base::Optional<quic::MessageStatus>));
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
  quic::QuicBufferAllocator* GetStreamSendBufferAllocator() override {
    return &allocator_;
  }

  TestWallClock& clock() { return clock_; }

 private:
  TestWallClock clock_;
  quic::SimpleBufferAllocator allocator_;
};

class QuicTransportEndToEndTest : public TestWithTaskEnvironment {
 public:
  QuicTransportEndToEndTest() {
    for (const quic::ParsedQuicVersion& version :
         QuicTransportClient::QuicVersionsForWebTransportOriginTrial()) {
      quic::QuicEnableVersion(version);
    }
    origin_ = url::Origin::Create(GURL{"https://example.org"});
    isolation_key_ =
        NetworkIsolationKey(SchemefulSite(origin_), SchemefulSite(origin_));

    URLRequestContextBuilder builder;
    builder.set_proxy_resolution_service(
        ConfiguredProxyResolutionService::CreateDirect());

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

    builder.set_net_log(&net_log_);
    context_ = builder.Build();

    // By default, quit on error instead of waiting for RunLoop() to time out.
    ON_CALL(visitor_, OnConnectionFailed()).WillByDefault([this]() {
      LOG(INFO) << "Connection failed: " << client_->error();
      run_loop_->Quit();
    });
    ON_CALL(visitor_, OnError()).WillByDefault([this]() {
      LOG(INFO) << "Connection error: " << client_->error();
      run_loop_->Quit();
    });
  }

  GURL GetURL(const std::string& suffix) {
    return GURL{base::StrCat({"quic-transport://test.example.com:",
                              base::NumberToString(port_), suffix})};
  }

  void StartServer(std::unique_ptr<quic::ProofSource> proof_source = nullptr) {
    if (proof_source == nullptr) {
      proof_source = quic::test::crypto_test_utils::ProofSourceForTesting();
    }
    server_ = std::make_unique<QuicTransportSimpleServer>(
        /* port */ 0, std::vector<url::Origin>({origin_}),
        std::move(proof_source));
    ASSERT_EQ(EXIT_SUCCESS, server_->Start());
    port_ = server_->server_address().port();
  }

  void Run() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  auto StopRunning() {
    return [this]() { run_loop_->Quit(); };
  }

 protected:
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<QuicTransportClient> client_;
  TestConnectionHelper* helper_;  // Owned by |context_|.
  ::testing::NiceMock<MockVisitor> visitor_;
  std::unique_ptr<QuicTransportSimpleServer> server_;
  std::unique_ptr<base::RunLoop> run_loop_;
  RecordingTestNetLog net_log_;

  int port_ = 0;
  url::Origin origin_;
  NetworkIsolationKey isolation_key_;
};

TEST_F(QuicTransportEndToEndTest, Connect) {
  StartServer();
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/discard"), origin_, &visitor_, isolation_key_, context_.get(),
      QuicTransportClient::Parameters());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();
  ASSERT_TRUE(client_->session() != nullptr);
  EXPECT_TRUE(client_->session()->IsSessionReady());
}

TEST_F(QuicTransportEndToEndTest, SendDatagram) {
  StartServer();
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/discard"), origin_, &visitor_, isolation_key_, context_.get(),
      QuicTransportClient::Parameters());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();
  ASSERT_TRUE(client_->session() != nullptr);
  EXPECT_TRUE(client_->session()->IsSessionReady());

  EXPECT_CALL(visitor_, OnDatagramProcessed(_)).Times(1);
  client_->session()->datagram_queue()->SendOrQueueDatagram(
      MemSliceFromString("test"));
}

TEST_F(QuicTransportEndToEndTest, EchoUnidirectionalStream) {
  StartServer();
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/echo"), origin_, &visitor_, isolation_key_, context_.get(),
      QuicTransportClient::Parameters());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();

  quic::QuicTransportClientSession* session = client_->session();
  ASSERT_TRUE(session != nullptr);
  ASSERT_TRUE(session->CanOpenNextOutgoingUnidirectionalStream());
  quic::QuicTransportStream* stream_out =
      session->OpenOutgoingUnidirectionalStream();
  EXPECT_TRUE(stream_out->Write("test"));
  EXPECT_TRUE(stream_out->SendFin());

  EXPECT_CALL(visitor_, OnIncomingUnidirectionalStreamAvailable())
      .WillOnce(StopRunning());
  Run();

  quic::QuicTransportStream* stream_in =
      session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream_in != nullptr);
  std::string data;
  stream_in->Read(&data);
  EXPECT_EQ("test", data);
}

TEST_F(QuicTransportEndToEndTest, CertificateFingerprint) {
  auto proof_source = std::make_unique<net::ProofSourceChromium>();
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  ASSERT_TRUE(proof_source->Initialize(
      certs_dir.AppendASCII("quic-short-lived.pem"),
      certs_dir.AppendASCII("quic-leaf-cert.key"),
      certs_dir.AppendASCII("quic-leaf-cert.key.sct")));
  StartServer(std::move(proof_source));
  // Set clock to a time in which quic-short-lived.pem is valid
  // (2020-06-05T20:35:00.000Z).
  helper_->clock().set_wall_now(
      quic::QuicWallTime::FromUNIXSeconds(1591389300));

  QuicTransportClient::Parameters parameters;
  parameters.server_certificate_fingerprints.push_back(
      quic::CertificateFingerprint{
          .algorithm = quic::CertificateFingerprint::kSha256,
          .fingerprint = "ED:3D:D7:C3:67:10:94:68:D1:DC:D1:26:5C:B2:74:D7:1C:"
                         "A2:63:3E:94:94:C0:84:39:D6:64:FA:08:B9:77:37"});
  client_ = std::make_unique<QuicTransportClient>(GetURL("/discard"), origin_,
                                                  &visitor_, isolation_key_,
                                                  context_.get(), parameters);
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();
  ASSERT_NE(client_->session(), nullptr);
  EXPECT_TRUE(client_->session()->IsSessionReady());
}

TEST_F(QuicTransportEndToEndTest, CertificateFingerprintValidityTooLong) {
  StartServer();
  QuicTransportClient::Parameters parameters;
  // The default QUIC test certificate is valid for ten years, which exceeds
  // the two-week limit.
  parameters.server_certificate_fingerprints.push_back(
      quic::CertificateFingerprint{
          .algorithm = quic::CertificateFingerprint::kSha256,
          .fingerprint = "25:17:B1:79:76:C8:94:BD:F0:B5:5C:0B:CC:70:C8:69:2B:"
                         "27:B8:84:F0:30:FE:A8:62:99:37:63:D2:A9:D6:EE"});
  client_ = std::make_unique<QuicTransportClient>(GetURL("/discard"), origin_,
                                                  &visitor_, isolation_key_,
                                                  context_.get(), parameters);
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnectionFailed()).WillOnce(StopRunning());
  Run();
  EXPECT_EQ(client_->session(), nullptr);
  EXPECT_THAT(client_->error().quic_error,
              quic::test::IsError(quic::QUIC_TLS_CERTIFICATE_UNKNOWN));
}

TEST_F(QuicTransportEndToEndTest, CertificateFingerprintMismatch) {
  StartServer();

  QuicTransportClient::Parameters parameters;
  parameters.server_certificate_fingerprints.push_back(
      quic::CertificateFingerprint{
          .algorithm = quic::CertificateFingerprint::kSha256,
          .fingerprint = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:"
                         "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00"});
  client_ = std::make_unique<QuicTransportClient>(GetURL("/discard"), origin_,
                                                  &visitor_, isolation_key_,
                                                  context_.get(), parameters);
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnectionFailed()).WillOnce(StopRunning());
  Run();
  EXPECT_EQ(client_->session(), nullptr);
  EXPECT_THAT(client_->error().quic_error,
              quic::test::IsError(quic::QUIC_TLS_CERTIFICATE_UNKNOWN));
}

TEST_F(QuicTransportEndToEndTest, OldVersion) {
  if (QuicTransportClient::QuicVersionsForWebTransportOriginTrial().size() ==
      1) {
    // This test shouldn't be run when there's only one supported version.
    return;
  }
  // Ensure all WebTransport versions are enabled except the first one.
  quic::QuicDisableVersion(
      QuicTransportClient::QuicVersionsForWebTransportOriginTrial().front());

  StartServer();
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/discard"), origin_, &visitor_, isolation_key_, context_.get(),
      QuicTransportClient::Parameters());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();
  ASSERT_TRUE(client_->session() != nullptr);
  EXPECT_TRUE(client_->session()->IsSessionReady());

  std::vector<NetLogEntry> events = net_log_.GetEntriesWithType(
      NetLogEventType::QUIC_SESSION_VERSION_NEGOTIATED);
  ASSERT_GE(events.size(), 1u);
  EXPECT_EQ(
      GetStringValueFromParams(events[0], "version"),
      quic::ParsedQuicVersionToString(
          QuicTransportClient::QuicVersionsForWebTransportOriginTrial()[1]));

  // Ensure the observer is set correctly after the version negotiation process.
  EXPECT_CALL(visitor_, OnDatagramProcessed(_)).Times(1);
  client_->session()->datagram_queue()->SendOrQueueDatagram(
      MemSliceFromString("test"));
}

}  // namespace
}  // namespace test
}  // namespace net
