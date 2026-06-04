// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/keypair.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto_test_utils_chromium.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_crypto_server_config_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/test_ticket_crypter.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

using test::IsOk;

namespace test {

namespace {

const char kResponseBody[] = "some arbitrary response body";

// Factory for creating HttpTransactions, used by TestTransactionConsumer.
class TestTransactionFactory : public HttpTransactionFactory {
 public:
  explicit TestTransactionFactory(
      const HttpNetworkSessionParams& session_params,
      const HttpNetworkSessionContext& session_context)
      : session_(std::make_unique<HttpNetworkSession>(session_params,
                                                      session_context)) {}

  ~TestTransactionFactory() override = default;

  // HttpTransactionFactory methods
  std::unique_ptr<HttpTransaction> CreateTransaction(
      RequestPriority priority) override {
    return std::make_unique<HttpNetworkTransaction>(priority, session_.get());
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return session_.get(); }

 private:
  std::unique_ptr<HttpNetworkSession> session_;
};

}  // namespace

class QuicEndToEndTest : public ::testing::Test, public WithTaskEnvironment {
 protected:
  explicit QuicEndToEndTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : WithTaskEnvironment(time_source),
        host_resolver_(CreateResolverImpl()),
        ssl_config_service_(
            std::make_unique<TestSSLConfigService>(SSLContextConfig())),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()) {
    std::unique_ptr<CertBuilder> cert =
        std::move(CertBuilder::CreateSimpleChain(1)[0]);
    cert->SetSubjectAltName("test.example.com");
    // ProofSourceChromium assumes the leaf cert uses an RSA key.
    cert->UseKeyFromFile(GetTestCertsDirectory().AppendASCII("rsa-2048-1.key"));
    cert_key_ = crypto::keypair::PrivateKey(
        bssl::UpRef(cert->GetKey()), crypto::SubtlePassKey::ForTesting());
    cert_ = cert->GetX509CertificateChain();

    request_.method = "GET";
    request_.url = GURL("https://test.example.com/");
    request_.load_flags = 0;
    request_.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    session_params_.enable_quic = true;

    session_context_.client_socket_factory =
        ClientSocketFactory::GetDefaultFactory();
    session_context_.quic_context = &quic_context_;
    session_context_.host_resolver = &host_resolver_;
    session_context_.cert_verifier = &cert_verifier_;
    session_context_.transport_security_state = &transport_security_state_;
    session_context_.proxy_resolution_service = proxy_resolution_service_.get();
    session_context_.ssl_config_service = ssl_config_service_.get();
    session_context_.http_user_agent_settings = &http_user_agent_settings_;
    session_context_.http_auth_handler_factory = auth_handler_factory_.get();
    session_context_.http_server_properties = &http_server_properties_;
  }

  // Creates a mock host resolver in which test.example.com
  // resolves to localhost.
  static std::unique_ptr<MockHostResolver> CreateResolverImpl() {
    auto resolver = std::make_unique<MockHostResolver>();
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    return resolver;
  }

  void SetUp() override {
    StartServer();

    // Use a mapped host resolver so that request for test.example.com (port 80)
    // reach the server running on localhost.
    std::string map_rule =
        "MAP test.example.com test.example.com:" +
        base::NumberToString(server_->server_address().port());
    EXPECT_TRUE(host_resolver_.AddRuleFromString(map_rule));

    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    quic_context_.params()->origins_to_force_quic_on.insert(
        url::SchemeHostPort("https", "test.example.com", 443));

    transaction_factory_ = std::make_unique<TestTransactionFactory>(
        session_params_, session_context_);
  }

  void TearDown() override {}

  // Starts the QUIC server listening on a random port.
  void StartServer() {
    std::unique_ptr<ProofSourceChromium> proof_source =
        std::make_unique<ProofSourceChromium>();
    proof_source->SetTicketCrypter(
        std::make_unique<quic::test::TestTicketCrypter>());
    ticket_crypter_ = reinterpret_cast<quic::test::TestTicketCrypter*>(
        proof_source->GetTicketCrypter());
    CertificateList cert_list;
    cert_list.push_back(cert_);
    proof_source->InitializeFromCertAndKey(cert_list, *cert_key_);
    server_address_ = IPEndPoint(IPAddress(127, 0, 0, 1), 0);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        quic::test::kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        quic::test::kInitialSessionFlowControlWindowForTest);
    server_ = std::make_unique<QuicSimpleServer>(
        std::move(proof_source), server_config_, server_config_options_,
        AllSupportedQuicVersions(), &memory_cache_backend_);
    server_->Listen(server_address_);
    server_address_ = server_->server_address();
    server_->StartReading();

    CertVerifyResult verify_result;
    verify_result.verified_cert = cert_list[0];
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
  }

  // Adds an entry to the cache used by the QUIC server to serve
  // responses.
  void AddToCache(std::string_view path,
                  int response_code,
                  std::string_view response_detail,
                  std::string_view body) {
    memory_cache_backend_.AddSimpleResponse("test.example.com", path,
                                            response_code, body);
  }

  // Populates |request_body_| with |length_| ASCII bytes.
  void GenerateBody(size_t length) {
    request_body_.clear();
    request_body_.reserve(length);
    for (size_t i = 0; i < length; ++i) {
      request_body_.append(1, static_cast<char>(32 + i % (126 - 32)));
    }
  }

  // Initializes |request_| for a post of |length| bytes.
  void InitializePostRequest(size_t length) {
    GenerateBody(length);
    std::vector<std::unique_ptr<UploadElementReader>> element_readers;
    element_readers.push_back(std::make_unique<UploadBytesElementReader>(
        base::as_byte_span(request_body_)));
    upload_data_stream_ = std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0);
    request_.method = "POST";
    request_.url = GURL("https://test.example.com/");
    request_.upload_data_stream = upload_data_stream_.get();
    ASSERT_THAT(request_.upload_data_stream->Init(CompletionOnceCallback(),
                                                  NetLogWithSource()),
                IsOk());
  }

  // Checks that |consumer| completed and received |status_line| and |body|.
  void CheckResponse(const TestTransactionConsumer& consumer,
                     const std::string& status_line,
                     const std::string& body) {
    ASSERT_TRUE(consumer.is_done());
    ASSERT_THAT(consumer.error(), IsOk());
    EXPECT_EQ(status_line, consumer.response_info()->headers->GetStatusLine());
    EXPECT_EQ(body, consumer.content());
  }

  quic::test::QuicFlagSaver saver_;
  QuicContext quic_context_;
  MappedHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  std::optional<crypto::keypair::PrivateKey> cert_key_;
  scoped_refptr<X509Certificate> cert_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"*", "test-ua"};
  HttpServerProperties http_server_properties_;
  HttpNetworkSessionParams session_params_;
  HttpNetworkSessionContext session_context_;
  std::unique_ptr<TestTransactionFactory> transaction_factory_;
  std::string request_body_;
  std::unique_ptr<UploadDataStream> upload_data_stream_;
  HttpRequestInfo request_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<QuicSimpleServer> server_;
  IPEndPoint server_address_;
  std::string server_hostname_;
  quic::QuicConfig server_config_;
  quic::QuicCryptoServerConfig::ConfigOptions server_config_options_;
  // Owned by server_.
  raw_ptr<quic::test::TestTicketCrypter> ticket_crypter_ = nullptr;
  bool strike_register_no_startup_period_ = false;
};

TEST_F(QuicEndToEndTest, LargeGetWithNoPacketLoss) {
  std::string response(10 * 1024, 'x');

  AddToCache(request_.url.PathForRequest(), 200, "OK", response);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", response);
}

TEST_F(QuicEndToEndTest, LargePostWithNoPacketLoss) {
  InitializePostRequest(1024 * 1024);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

TEST_F(QuicEndToEndTest, LargePostWithPacketLoss) {
  // FLAGS_fake_packet_loss_percentage = 30;
  InitializePostRequest(1024 * 1024);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

TEST_F(QuicEndToEndTest, UberTest) {
  // FLAGS_fake_packet_loss_percentage = 30;

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  std::vector<std::unique_ptr<TestTransactionConsumer>> consumers;
  for (size_t i = 0; i < 100; ++i) {
    TestTransactionConsumer* consumer = new TestTransactionConsumer(
        DEFAULT_PRIORITY, transaction_factory_.get());
    consumers.push_back(base::WrapUnique(consumer));
    consumer->Start(&request_, NetLogWithSource());
  }

  for (const auto& consumer : consumers)
    CheckResponse(*consumer.get(), "HTTP/1.1 200", kResponseBody);
}

TEST_F(QuicEndToEndTest, EnableMLKEM) {
  // Configure the server to only support ML-KEM.
  server_->crypto_config()->set_preferred_groups({SSL_GROUP_X25519_MLKEM768});

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
  EXPECT_EQ(consumer.response_info()->ssl_info.key_exchange_group,
            SSL_GROUP_X25519_MLKEM768);
}

TEST_F(QuicEndToEndTest, CryptoHandshakeCompleteMetrics) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // The Net.QuicSession.HandshakeConfirmedTime metric should be logged.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime", 1);

  // MTC variants of that metric (and related MTC metrics) should not be
  // present.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime.MTC", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.MTC.NewConnection", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.MTC.Resumption", 0);
  histograms.ExpectTotalCount("Net.QuicSession.TLSHandshakeBytes.MTC2", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.TLSHandshakeBytes.MTC2.NewConnection", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.TLSHandshakeBytes.MTC2.Resumption", 0);
}

TEST_F(QuicEndToEndTest, ServerHandshakePaddingMetrics) {
  SetQuicRestartFlag(tls_server_padding_support, true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "128"}});

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // The Net.QuicSession.HandshakeConfirmedTime metric should be logged.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime", 1);

  // The server padding metric should also be logged.
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.ServerPadding", 1);
}

TEST_F(QuicEndToEndTest, ServerHandshakePaddingMetricsZeroPadding) {
  SetQuicRestartFlag(tls_server_padding_support, true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "0"}});

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // The Net.QuicSession.HandshakeConfirmedTime metric should be logged.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime", 1);

  // The server padding metric should also be logged.
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.ServerPadding", 1);
}

TEST_F(QuicEndToEndTest, ServerHandshakePaddingMetricsNoServerSupport) {
  SetQuicRestartFlag(tls_server_padding_support, false);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "0"}});

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // The Net.QuicSession.HandshakeConfirmedTime metric should be logged.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime", 1);

  // The server padding metric should not be logged.
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.ServerPadding", 0);
}

TEST_F(QuicEndToEndTest,
       ServerHandshakePaddingMetricsServerSupportFeatureDisabled) {
  SetQuicRestartFlag(tls_server_padding_support, false);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAddTLSServerHandshakePadding);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // The Net.QuicSession.HandshakeConfirmedTime metric should be logged.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime", 1);

  // The server padding metric should not be logged.
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.ServerPadding", 0);
}

TEST_F(QuicEndToEndTest, ProofVerifyDetailsMetrics) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  // Check that MTC-related metrics that are conditionally logged in
  // OnProofVerifyDetailsAvailable don't get logged when the server doesn't
  // support MTCs.
  histograms.ExpectTotalCount("Net.QuicSession.MTCMetadataAge2", 0);
  histograms.ExpectTotalCount("Net.QuicSession.HasMTCMetadata2", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.CertVerificationResult.MTCAdvertised2", 0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.CertVerificationResult.MTCReceived2", 0);
  histograms.ExpectTotalCount("Net.QuicSession.MTCResult2", 0);
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class QuicEndToEndMTCTest : public QuicEndToEndTest {
 public:
  QuicEndToEndMTCTest()
      : QuicEndToEndTest(base::test::TaskEnvironment::TimeSource::DEFAULT) {
    feature_list_.InitWithFeatures(
        {{features::kTLSTrustAnchorIDs, features::kVerifyMTCs}}, {});
  }

  void SetUp() override {
    auto leaf = std::move(CertBuilder::CreateSimpleChain(1)[0]);
    leaf->SetSubjectAltName("test.example.com");
    // ProofSourceChromium assumes the leaf cert uses an RSA key.
    ASSERT_TRUE(leaf->UseKeyFromFile(
        GetTestCertsDirectory().AppendASCII("rsa-2048-1.key")));
    cert_key_ = crypto::keypair::PrivateKey(
        bssl::UpRef(leaf->GetKey()), crypto::SubtlePassKey::ForTesting());

    // This is the log ID for the MTC experiment (see log_id in
    // root_store.textproto). It doesn't actually matter for this test.
    constexpr uint8_t kMtcLogId[] = {0x82, 0xda, 0x4b, 0x30, 0x08};
    // This is the base ID for the MTC experiment (see kMtcExperimentBaseId in
    // quic_chromium_client_session.cc).
    constexpr uint8_t kMtcBaseId[] = {0x82, 0xda, 0x4b, 0x30, 0x07};
    MtcLogBuilder mtc_log(kMtcLogId, kMtcBaseId);
    uint64_t leaf_index = mtc_log.AddEntry(*leaf);
    mtc_log.AdvanceLandmark();
    auto leaf_der = mtc_log.CreateSignaturelessCertificate(leaf_index);
    ASSERT_TRUE(leaf_der);
    cert_ = X509Certificate::CreateFromBytes(*leaf_der);
    ASSERT_TRUE(cert_);

    // This is a fake trust anchor ID formed from the base ID above. (We could
    // make one that actually matches the generated landmark number from
    // mtc_log, but it doesn't matter for this test.)
    const auto kMtcTrustAnchorId =
        x509_util::AppendOidComponent(kMtcBaseId, 0x01);

    SSLContextConfig config;
    config.mtc_trust_anchor_ids = {{kMtcTrustAnchorId}};
    config.mtc_update_time_seconds =
        (base::Time::Now() - kMtcUpdateAge).InMillisecondsSinceUnixEpoch() /
        1000;
    ssl_config_service_->UpdateSSLConfigAndNotify(config);

    QuicEndToEndTest::SetUp();
  }

  static constexpr base::TimeDelta kMtcUpdateAge = base::Hours(42);

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(QuicEndToEndMTCTest, SimpleConnection) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  EXPECT_EQ(consumer.response_info()->ssl_info.cert->signature_algorithm(),
            bssl::SignatureAlgorithm::kMtcProofDraftDavidben08);

  // Not logged, since the server is not configured to reply with a TAI.
  // TODO(crbug.com/482083310): add tests of other cases.
  histograms.ExpectTotalCount("Net.QuicSession.MTCLandmarkDelta2.OldClient", 0);
  histograms.ExpectTotalCount("Net.QuicSession.MTCLandmarkDelta2.CurrentClient",
                              0);

  histograms.ExpectTimeBucketCount("Net.QuicSession.MTCMetadataAge2",
                                   kMtcUpdateAge, 1);

  histograms.ExpectUniqueSample("Net.QuicSession.HasMTCMetadata2", /*sample=*/1,
                                /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample("Net.QuicSession.MTCResult2",
                                /*sample=*/MTCResult::kValidMTC,
                                /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample(
      "Net.QuicSession.CertVerificationResult.MTCAdvertised2",
      /*sample=*/-net::OK,
      /*expected_bucket_count=*/1);

  histograms.ExpectUniqueSample(
      "Net.QuicSession.CertVerificationResult.MTCReceived2",
      /*sample=*/-net::OK,
      /*expected_bucket_count=*/1);

  // We don't know what the exact timing value should be for
  // HandshakeConfirmedTime, so only check that a sample is present.
  histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime.MTC", 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.MTC.NewConnection", 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.HandshakeConfirmedTime.MTC.Resumption", 0);

  // Should be logged, but we don't know what the exact value will be, so just
  // check that a sample is present.
  histograms.ExpectTotalCount("Net.QuicSession.TLSHandshakeBytes.MTC2", 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.TLSHandshakeBytes.MTC2.NewConnection", 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.TLSHandshakeBytes.MTC2.Resumption", 0);
}

TEST_F(QuicEndToEndMTCTest, MTCResultResumptionNotAttempted) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  base::HistogramTester histograms;
  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());
  ASSERT_NO_FATAL_FAILURE(
      CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

  histograms.ExpectUniqueSample("Net.QuicSession.MTCResult2",
                                MTCResult::kValidMTC, 1);
}

class QuicEndToEndMTCResumptionTest
    : public QuicEndToEndMTCTest,
      public ::testing::WithParamInterface<bool> {
  void SetUp() override {
    QuicEndToEndMTCTest::SetUp();

    quic::test::QuicCryptoServerConfigPeer peer(server_->crypto_config());
    peer.mutable_ssl_config()->early_data_enabled = GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(All, QuicEndToEndMTCResumptionTest, ::testing::Bool());

TEST_P(QuicEndToEndMTCResumptionTest, MTCResultResumptionAccepted) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  // First request to get a session ticket.
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
  }

  // Force a second QUIC connection.
  transaction_factory_->GetSession()->quic_session_pool()->CloseAllSessions(
      ERR_FAILED, quic::QUIC_NO_ERROR);

  // Second request should attempt resumption and succeed.
  base::HistogramTester histograms;
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
    EXPECT_EQ(consumer.response_info()->ssl_info.early_data_accepted,
              GetParam());
  }

  // Check that MTCResult is not logged because there was no certificate sent by
  // the server on this connection.
  histograms.ExpectTotalCount("Net.QuicSession.MTCResult2", 0);
}

TEST_P(QuicEndToEndMTCResumptionTest, MTCResultResumptionRejected) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  // First request to get a session ticket.
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
  }

  // Force a second QUIC connection.
  transaction_factory_->GetSession()->quic_session_pool()->CloseAllSessions(
      ERR_FAILED, quic::QUIC_NO_ERROR);

  // Configure server to reject the ticket.
  ticket_crypter_->set_fail_decrypt(true);

  // Second request should attempt resumption but fail, falling back to a full
  // handshake.
  base::HistogramTester histograms;
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
    EXPECT_FALSE(consumer.response_info()->ssl_info.early_data_accepted);
  }

  // Even though resumption was attempted, it was rejected, so MTCResult should
  // be logged because we received a cert (an MTC cert) from the server.
  histograms.ExpectUniqueSample("Net.QuicSession.MTCResult2",
                                MTCResult::kValidMTC, 1);
}

TEST_P(QuicEndToEndMTCResumptionTest, HandshakeBytesAndConfirmedTime) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  // First request (full handshake).
  {
    base::HistogramTester histograms;

    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

    // HandshakeConfirmedTime and TLSHandshakeBytes should be recorded (and with
    // the NewConnection variant).
    histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime.MTC",
                                1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.HandshakeConfirmedTime.MTC.NewConnection", 1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.HandshakeConfirmedTime.MTC.Resumption", 0);
    histograms.ExpectTotalCount("Net.QuicSession.TLSHandshakeBytes.MTC2", 1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.TLSHandshakeBytes.MTC2.NewConnection", 1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.TLSHandshakeBytes.MTC2.Resumption", 0);
  }

  // Force a second QUIC connection.
  transaction_factory_->GetSession()->quic_session_pool()->CloseAllSessions(
      ERR_FAILED, quic::QUIC_NO_ERROR);

  // Second request (resumption).
  {
    base::HistogramTester histograms;

    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));

    // HandshakeConfirmedTime and TLSHandshakeBytes should be recorded again
    // (this time with the Resumption variant).
    histograms.ExpectTotalCount("Net.QuicSession.HandshakeConfirmedTime.MTC",
                                1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.HandshakeConfirmedTime.MTC.NewConnection", 0);
    histograms.ExpectTotalCount(
        "Net.QuicSession.HandshakeConfirmedTime.MTC.Resumption", 1);
    histograms.ExpectTotalCount("Net.QuicSession.TLSHandshakeBytes.MTC2", 1);
    histograms.ExpectTotalCount(
        "Net.QuicSession.TLSHandshakeBytes.MTC2.NewConnection", 0);
    histograms.ExpectTotalCount(
        "Net.QuicSession.TLSHandshakeBytes.MTC2.Resumption", 1);
  }
}

TEST_F(QuicEndToEndMTCTest, MTCResultZeroRttRejectedResumptionAccepted) {
  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  // First request to get a session ticket.
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
  }

  // Force a second QUIC connection.
  transaction_factory_->GetSession()->quic_session_pool()->CloseAllSessions(
      ERR_FAILED, quic::QUIC_NO_ERROR);

  // Disable early data on the server.
  quic::test::QuicCryptoServerConfigPeer peer(server_->crypto_config());
  peer.mutable_ssl_config()->early_data_enabled = false;

  // Second request should attempt 0-RTT, have it rejected, but then succeed
  // with 1-RTT resumption.
  base::HistogramTester histograms;
  {
    TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                     transaction_factory_.get());
    consumer.Start(&request_, NetLogWithSource());
    ASSERT_NO_FATAL_FAILURE(
        CheckResponse(consumer, "HTTP/1.1 200", kResponseBody));
    EXPECT_FALSE(consumer.response_info()->ssl_info.early_data_accepted);
  }

  // Even though 0-RTT was rejected, a TLS session resumption was still
  // successful, so no cert was sent on the wire and no MTCResult should be
  // logged.
  histograms.ExpectTotalCount("Net.QuicSession.MTCResult2", 0);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

}  // namespace test
}  // namespace net
