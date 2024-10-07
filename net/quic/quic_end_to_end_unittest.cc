// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/ip_address.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
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
#include "net/quic/crypto_test_utils_chromium.h"
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
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
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override {
    *trans = std::make_unique<HttpNetworkTransaction>(priority, session_.get());
    return OK;
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return session_.get(); }

 private:
  std::unique_ptr<HttpNetworkSession> session_;
};

}  // namespace

class QuicEndToEndTest : public ::testing::Test, public WithTaskEnvironment {
 protected:
  QuicEndToEndTest()
      : host_resolver_(CreateResolverImpl()),
        ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()) {
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

    CertVerifyResult verify_result;
    verify_result.verified_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "quic-chain.pem");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
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
        HostPortPair::FromString("test.example.com:443"));

    transaction_factory_ = std::make_unique<TestTransactionFactory>(
        session_params_, session_context_);
  }

  void TearDown() override {}

  // Starts the QUIC server listening on a random port.
  void StartServer() {
    server_address_ = IPEndPoint(IPAddress(127, 0, 0, 1), 0);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        quic::test::kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        quic::test::kInitialSessionFlowControlWindowForTest);
    server_ = std::make_unique<QuicSimpleServer>(
        net::test::ProofSourceForTestingChromium(), server_config_,
        server_config_options_, AllSupportedQuicVersions(),
        &memory_cache_backend_);
    server_->Listen(server_address_);
    server_address_ = server_->server_address();
    server_->StartReading();
    server_started_ = true;
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

  QuicContext quic_context_;
  MappedHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
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
  bool server_started_;
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

// crbug.com/559173
#if defined(THREAD_SANITIZER)
TEST_F(QuicEndToEndTest, DISABLED_LargePostWithNoPacketLoss) {
#else
TEST_F(QuicEndToEndTest, LargePostWithNoPacketLoss) {
#endif
  InitializePostRequest(1024 * 1024);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

// crbug.com/559173
#if defined(THREAD_SANITIZER)
TEST_F(QuicEndToEndTest, DISABLED_LargePostWithPacketLoss) {
#else
TEST_F(QuicEndToEndTest, LargePostWithPacketLoss) {
#endif
  // FLAGS_fake_packet_loss_percentage = 30;
  InitializePostRequest(1024 * 1024);

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  CheckResponse(consumer, "HTTP/1.1 200", kResponseBody);
}

// crbug.com/536845
#if defined(THREAD_SANITIZER)
TEST_F(QuicEndToEndTest, DISABLED_UberTest) {
#else
TEST_F(QuicEndToEndTest, UberTest) {
#endif
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
  // Enable ML-KEM on the client.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kPostQuantumKyber}, {});

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

TEST_F(QuicEndToEndTest, MLKEMDisabled) {
  // Disable ML-KEM on the client.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {features::kPostQuantumKyber});

  // Configure the server to only support ML-KEM.
  server_->crypto_config()->set_preferred_groups({SSL_GROUP_X25519_MLKEM768});

  AddToCache(request_.url.PathForRequest(), 200, "OK", kResponseBody);

  TestTransactionConsumer consumer(DEFAULT_PRIORITY,
                                   transaction_factory_.get());
  consumer.Start(&request_, NetLogWithSource());

  // Connection should fail because there's no supported group in common between
  // client and server.
  EXPECT_EQ(consumer.error(), net::ERR_QUIC_PROTOCOL_ERROR);
}

}  // namespace test
}  // namespace net
