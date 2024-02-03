// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/third_party/quiche/src/quiche/binary_http/binary_http_message.h"
#include "net/third_party/quiche/src/quiche/common/quiche_data_writer.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/oblivious_http_request_handler.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace {

const char kRelayURL[] = "https://relay.test:13/";
const char kResourceURL[] = "https://resource.test:37/path";

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
const uint8_t kTestPrivateKey[] = {
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
};

const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

testing::Matcher<net::HttpRequestHeaders::HeaderVector> UnorderedHeadersAre(
    const net::HttpRequestHeaders::HeaderVector& expected) {
  std::vector<testing::Matcher<net::HttpRequestHeaders::HeaderKeyValuePair>> ms;
  for (const auto& expected_pair : expected) {
    ms.emplace_back(testing::AllOf(
        testing::Field("key", &net::HttpRequestHeaders::HeaderKeyValuePair::key,
                       expected_pair.key),
        testing::Field("value",
                       &net::HttpRequestHeaders::HeaderKeyValuePair::value,
                       expected_pair.value)));
  }
  return testing::UnorderedElementsAreArray(ms);
}

class TestOhttpClient : public network::mojom::ObliviousHttpClient {
 public:
  enum class ResponseType { kSuccess, kNetError, kOuterResponseErrorCode };

  TestOhttpClient() : receiver_(this) {}

  void SetExpectedNetError(int expected_net_error) {
    expected_response_type_ = ResponseType::kNetError;
    expected_net_error_ = expected_net_error;
  }

  void SetExpectedOuterResponseErrorCode(
      int expected_outer_response_error_code) {
    expected_response_type_ = ResponseType::kOuterResponseErrorCode;
    expected_outer_response_error_code_ = expected_outer_response_error_code;
  }

  void SetExpectedInnerResponse(
      int expected_inner_response_code,
      std::string expected_body,
      std::multimap<std::string, std::string> expected_headers) {
    expected_response_type_ = ResponseType::kSuccess;
    expected_inner_response_code_ = expected_inner_response_code;
    expected_body_ = expected_body;
    expected_headers_ = std::move(expected_headers);
  }

  mojo::PendingRemote<network::mojom::ObliviousHttpClient>
  CreatePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnCompleted(
      network::mojom::ObliviousHttpCompletionResultPtr status) override {
    is_on_completed_called_ = true;
    switch (expected_response_type_) {
      case ResponseType::kSuccess: {
        ASSERT_TRUE(status->is_inner_response());
        EXPECT_EQ(expected_inner_response_code_,
                  status->get_inner_response()->response_code);
        EXPECT_EQ(expected_body_, status->get_inner_response()->response_body);
        // Verify headers.
        size_t iter = 0;
        std::string name;
        std::string value;
        std::multimap<std::string, std::string> actual_headers;
        while (status->get_inner_response()->headers->EnumerateHeaderLines(
            &iter, &name, &value)) {
          actual_headers.insert(
              std::pair<std::string, std::string>(name, value));
        }
        EXPECT_EQ(expected_headers_, actual_headers);
        break;
      }
      case ResponseType::kNetError: {
        ASSERT_TRUE(status->is_net_error());
        EXPECT_EQ(expected_net_error_, status->get_net_error());
        break;
      }
      case ResponseType::kOuterResponseErrorCode: {
        ASSERT_TRUE(status->is_outer_response_error_code());
        EXPECT_EQ(expected_outer_response_error_code_,
                  status->get_outer_response_error_code());
        break;
      }
    }
    run_loop_.Quit();
  }

  void WaitForCall() { run_loop_.Run(); }

  bool IsOnCompletedCalled() { return is_on_completed_called_; }

 private:
  ResponseType expected_response_type_;
  int expected_inner_response_code_ = 0;
  std::string expected_body_;
  std::multimap<std::string, std::string> expected_headers_;
  int expected_outer_response_error_code_ = 0;
  int expected_net_error_ = 0;
  bool is_on_completed_called_ = false;
  mojo::Receiver<network::mojom::ObliviousHttpClient> receiver_;
  base::RunLoop run_loop_;
};

}  // namespace

class TestObliviousHttpRequestHandler : public testing::Test {
 public:
  TestObliviousHttpRequestHandler()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::IO,
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        network_service_(network::NetworkService::CreateForTesting()),
        loader_factory_receiver_(&loader_factory_) {
    network::mojom::NetworkContextParamsPtr context_params =
        network::CreateNetworkContextParamsForTesting();
    context_params->cert_verifier_params =
        network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_context_ = std::make_unique<network::NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params),
        network::NetworkContext::OnConnectionCloseCallback());

    auto key_configs = quiche::ObliviousHttpKeyConfigs::ParseConcatenatedKeys(
        CreateTestKeyConfig());
    CHECK(key_configs.ok()) << key_configs.status();
    quiche::ObliviousHttpHeaderKeyConfig key_config =
        key_configs->PreferredConfig();
    auto ohttp_gateway = quiche::ObliviousHttpGateway::Create(
        std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                    sizeof(kTestPrivateKey)),
        key_config);
    CHECK(ohttp_gateway.ok()) << ohttp_gateway.status();
    ohttp_gateway_ = std::move(ohttp_gateway).value();
  }

  network::NetworkContext* network_context() { return network_context_.get(); }
  network::TestURLLoaderFactory* loader_factory() { return &loader_factory_; }

  std::unique_ptr<network::ObliviousHttpRequestHandler> CreateHandler() {
    auto handler = std::make_unique<network::ObliviousHttpRequestHandler>(
        network_context());
    handler->SetURLLoaderFactoryForTesting(
        loader_factory_receiver_.BindNewPipeAndPassRemote());
    return handler;
  }

  std::string DecryptRequest(std::string cipher_text) {
    auto request = ohttp_gateway_->DecryptObliviousHttpRequest(cipher_text);
    EXPECT_TRUE(request.ok()) << request.status();
    return std::string(request->GetPlaintextData());
  }

  void RespondToPendingRequest(std::string body,
                               std::multimap<std::string, std::string> headers,
                               GURL relay_url = GURL(kRelayURL),
                               net::HttpStatusCode status = net::HTTP_OK) {
    const network::ResourceRequest* pending_request;
    ASSERT_TRUE(
        loader_factory()->IsPending(relay_url.spec(), &pending_request));

    ASSERT_TRUE(pending_request->request_body);
    ASSERT_EQ(1u, pending_request->request_body->elements()->size());

    std::string request_body =
        std::string(pending_request->request_body->elements()
                        ->at(0)
                        .As<network::DataElementBytes>()
                        .AsStringPiece());

    auto request = ohttp_gateway_->DecryptObliviousHttpRequest(request_body);
    ASSERT_TRUE(request.ok()) << request.status();
    auto ohttp_context = std::move(*request).ReleaseContext();

    quiche::BinaryHttpResponse bhttp_response(status);
    bhttp_response.set_body(std::move(body));
    for (auto kv : headers) {
      bhttp_response.AddHeaderField({kv.first, kv.second});
    }
    auto payload = bhttp_response.Serialize();
    ASSERT_TRUE(payload.ok()) << payload.status();

    auto response =
        ohttp_gateway_->CreateObliviousHttpResponse(*payload, ohttp_context);
    ASSERT_TRUE(response.ok()) << response.status();

    EXPECT_TRUE(loader_factory()->SimulateResponseForPendingRequest(
        /*url=*/relay_url,
        /*completion_status=*/network::URLLoaderCompletionStatus(),
        /*response_head=*/network::CreateURLResponseHead(net::HTTP_OK),
        /*content=*/response->EncapsulateAndSerialize()));
  }

  std::string CreateTestKeyConfig() {
    const size_t kOHTTPKeySizeBytes =
        1 + 2 + sizeof(kTestPublicKey) + 2 + 2 + 2;
    std::string ohttp_key_config(kOHTTPKeySizeBytes, '\0');
    quiche::QuicheDataWriter writer(ohttp_key_config.size(),
                                    ohttp_key_config.data());
    EXPECT_TRUE(writer.WriteUInt8('K'));  // Key ID can be arbitrary.
    EXPECT_TRUE(writer.WriteUInt16(EVP_HPKE_DHKEM_X25519_HKDF_SHA256));
    EXPECT_TRUE(writer.WriteBytes(&kTestPublicKey[0], sizeof(kTestPublicKey)));
    EXPECT_TRUE(writer.WriteUInt16(4u));
    EXPECT_TRUE(writer.WriteUInt16(EVP_HPKE_HKDF_SHA256));
    EXPECT_TRUE(writer.WriteUInt16(EVP_HPKE_AES_256_GCM));
    EXPECT_EQ(ohttp_key_config.length(), writer.length());
    return ohttp_key_config;
  }

  network::mojom::ObliviousHttpRequestPtr CreateRequest() {
    network::mojom::ObliviousHttpRequestPtr request =
        CreateRequestWithoutBody();
    request->request_body = network::mojom::ObliviousHttpRequestBody::New(
        /*content=*/"test data", /*content_type=*/"application/testdata");
    return request;
  }

  network::mojom::ObliviousHttpRequestPtr CreateRequestWithoutBody() {
    network::mojom::ObliviousHttpRequestPtr request =
        network::mojom::ObliviousHttpRequest::New();
    request->relay_url = GURL(kRelayURL);
    request->key_config = CreateTestKeyConfig();
    request->resource_url = GURL(kResourceURL);
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    return request;
  }

  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
    task_environment_.RunUntilIdle();
  }

  void VerifyNetLog(const net::RecordingNetLogObserver& net_log_observer,
                    bool expected_has_response_data_and_headers,
                    int expected_net_error,
                    std::optional<int> expected_outer_response_error_code,
                    std::optional<int> expected_inner_response_code) {
    auto entries = net_log_observer.GetEntries();
    size_t pos = net::ExpectLogContainsSomewhereAfter(
        entries, /*start_offset=*/0,
        net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST,
        net::NetLogEventPhase::BEGIN);
    pos = net::ExpectLogContainsSomewhere(
        entries, /*start_offset=*/pos + 1,
        net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST_DATA,
        net::NetLogEventPhase::NONE);
    EXPECT_TRUE(
        net::GetOptionalIntegerValueFromParams(entries[pos], "byte_count"));
    if (expected_has_response_data_and_headers) {
      pos = net::ExpectLogContainsSomewhereAfter(
          entries, /*start_offset=*/pos + 1,
          net::NetLogEventType::OBLIVIOUS_HTTP_RESPONSE_DATA,
          net::NetLogEventPhase::NONE);
      EXPECT_TRUE(
          net::GetOptionalIntegerValueFromParams(entries[pos], "byte_count"));
      pos = net::ExpectLogContainsSomewhereAfter(
          entries, /*start_offset=*/pos + 1,
          net::NetLogEventType::OBLIVIOUS_HTTP_RESPONSE_HEADERS,
          net::NetLogEventPhase::NONE);
      EXPECT_TRUE(entries[pos].params.FindList("headers"));
    }
    pos = net::ExpectLogContainsSomewhereAfter(
        entries, /*start_offset=*/pos + 1,
        net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST,
        net::NetLogEventPhase::END);
    EXPECT_EQ(expected_net_error,
              net::GetOptionalNetErrorCodeFromParams(entries[pos]));
    EXPECT_EQ(expected_outer_response_error_code,
              net::GetOptionalIntegerValueFromParams(
                  entries[pos], "outer_response_error_code"));
    EXPECT_EQ(expected_inner_response_code,
              net::GetOptionalIntegerValueFromParams(entries[pos],
                                                     "inner_response_code"));
  }

 private:
  std::optional<quiche::ObliviousHttpGateway> ohttp_gateway_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;
  network::TestURLLoaderFactory loader_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> loader_factory_receiver_;
};

TEST_F(TestObliviousHttpRequestHandler, TestDisconnect) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"",
        /*expected_headers=*/{});
    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
  }

  {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"",
        /*expected_headers=*/{});
    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    RespondToPendingRequest("", {});
    client.WaitForCall();
  }
  // Empty body
  {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"",
        /*expected_headers=*/{});
    handler->StartRequest(CreateRequestWithoutBody(),
                          client.CreatePendingRemote());
    RespondToPendingRequest("", {});
    client.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, TestInvalidArguments) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request =
        network::mojom::ObliviousHttpRequest::New();

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Relay URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->relay_url = GURL();

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Relay URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->resource_url = GURL();

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Resource URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->traffic_annotation = net::MutableNetworkTrafficAnnotationTag();

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Traffic Annotation", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->method = std::string(17, 'A');

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Method", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->request_body->content = std::string(5 * 1024 * 1024 + 1, ' ');

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Request body too large", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->request_body->content_type = std::string(257, ' ');

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Content-Type too large", obs.WaitForBadMessage());
  }
}

TEST_F(TestObliviousHttpRequestHandler, TestRequestFormat) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    net::RecordingNetLogObserver net_log_observer;
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"response body",
        /*expected_headers=*/
        {{"cache-control", "s-maxage=3600"}, {"content-type", "text/html"}});

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    const network::ResourceRequest* pending_request;
    ASSERT_TRUE(loader_factory()->IsPending(kRelayURL, &pending_request));
    EXPECT_EQ(net::HttpRequestHeaders::kPostMethod, pending_request->method);
    EXPECT_EQ(network::mojom::RedirectMode::kError,
              pending_request->redirect_mode);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              pending_request->credentials_mode);
    EXPECT_TRUE(pending_request->site_for_cookies.IsNull());
    EXPECT_FALSE(pending_request->trust_token_params);
    EXPECT_THAT(
        pending_request->headers.GetHeaderVector(),
        UnorderedHeadersAre({
            {net::HttpRequestHeaders::kContentType, "message/ohttp-req"},
        }));
    ASSERT_TRUE(pending_request->request_body);
    ASSERT_EQ(1u, pending_request->request_body->elements()->size());

    std::string body = std::string(pending_request->request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());
    std::string plain_text_body = DecryptRequest(body);

    auto maybe_request = quiche::BinaryHttpRequest::Create(plain_text_body);
    ASSERT_TRUE(maybe_request.ok()) << maybe_request.status();
    quiche::BinaryHttpRequest request = std::move(maybe_request).value();
    EXPECT_EQ(request.control_data().method, "GET");
    EXPECT_EQ(request.control_data().scheme, "https");
    EXPECT_EQ(request.control_data().authority, "");  // Stored in headers.
    EXPECT_EQ(request.control_data().path, "/path");
    EXPECT_THAT(request.GetHeaderFields(),
                testing::IsSupersetOf<quiche::BinaryHttpRequest::Field>({
                    {"host", "resource.test:37"},
                    {"content-length", "9"},
                    {"content-type", "application/testdata"},
                }));
    EXPECT_EQ(request.body(), "test data");
    RespondToPendingRequest(
        "response body",
        {{"cache-control", "s-maxage=3600"}, {"content-type", "text/html"}});
    client.WaitForCall();

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/true,
                 /*expected_net_error=*/net::OK,
                 /*expected_outer_response_error_code=*/std::nullopt,
                 /*expected_inner_response_code=*/net::HTTP_OK);
  }
}

TEST_F(TestObliviousHttpRequestHandler, TestTimeout) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  // Default timeout.
  {
    net::RecordingNetLogObserver net_log_observer;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_TIMED_OUT);

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());

    FastForward(base::Seconds(59));
    EXPECT_FALSE(client.IsOnCompletedCalled());
    FastForward(base::Seconds(1));
    EXPECT_TRUE(client.IsOnCompletedCalled());

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/false,
                 /*expected_net_error=*/net::ERR_TIMED_OUT,
                 /*expected_outer_response_error_code=*/std::nullopt,
                 /*expected_inner_response_code=*/std::nullopt);
  }
  // Configured timeout.
  {
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_TIMED_OUT);

    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->timeout_duration = base::Seconds(3);

    handler->StartRequest(std::move(request), client.CreatePendingRemote());

    FastForward(base::Seconds(2));
    EXPECT_FALSE(client.IsOnCompletedCalled());
    FastForward(base::Seconds(1));
    EXPECT_TRUE(client.IsOnCompletedCalled());
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesOuterHttpError) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    net::RecordingNetLogObserver net_log_observer;
    loader_factory()->AddResponse(kRelayURL, "", net::HTTP_NOT_FOUND);
    TestOhttpClient client;
    client.SetExpectedOuterResponseErrorCode(net::HTTP_NOT_FOUND);

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    client.WaitForCall();

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/false,
                 /*expected_net_error=*/net::ERR_HTTP_RESPONSE_CODE_FAILURE,
                 /*expected_outer_response_error_code=*/net::HTTP_NOT_FOUND,
                 /*expected_inner_response_code=*/std::nullopt);
  }
  {
    net::RecordingNetLogObserver net_log_observer;
    loader_factory()->AddResponse(
        GURL(kRelayURL), network::CreateURLResponseHead(net::HTTP_OK), "",
        network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET),
        network::TestURLLoaderFactory::Redirects(),
        network::TestURLLoaderFactory::ResponseProduceFlags::
            kSendHeadersOnNetworkError);
    TestOhttpClient client;
    // The outer HTTP error code should be set only when the net error is
    // ERR_HTTP_RESPONSE_CODE_FAILURE. Otherwise, log the net error instead.
    client.SetExpectedNetError(net::ERR_CONNECTION_RESET);

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    client.WaitForCall();

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/false,
                 /*expected_net_error=*/net::ERR_CONNECTION_RESET,
                 /*expected_outer_response_error_code=*/std::nullopt,
                 /*expected_inner_response_code=*/std::nullopt);
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesInnerHttpError) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    net::RecordingNetLogObserver net_log_observer;
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_NOT_FOUND,
        /*expected_body=*/"",
        /*expected_headers=*/
        {{"cache-control", "s-maxage=60"}});

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    RespondToPendingRequest("", {{"cache-control", "s-maxage=60"}},
                            GURL(kRelayURL), net::HTTP_NOT_FOUND);
    client.WaitForCall();

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/true,
                 /*expected_net_error=*/net::OK,
                 /*expected_outer_response_error_code=*/std::nullopt,
                 /*expected_inner_response_code=*/net::HTTP_NOT_FOUND);
  }
  {
    net::RecordingNetLogObserver net_log_observer;
    TestOhttpClient client;
    client.SetExpectedNetError(net::ERR_INVALID_RESPONSE);

    handler->StartRequest(CreateRequest(), client.CreatePendingRemote());
    ASSERT_TRUE(loader_factory()->IsPending(kRelayURL));
    EXPECT_TRUE(loader_factory()->SimulateResponseForPendingRequest(
        /*url=*/GURL(kRelayURL),
        /*completion_status=*/network::URLLoaderCompletionStatus(),
        /*response_head=*/network::CreateURLResponseHead(net::HTTP_OK),
        /*content=*/"malformed inner response"));
    client.WaitForCall();

    VerifyNetLog(net_log_observer,
                 /*expected_has_response_data_and_headers=*/false,
                 /*expected_net_error=*/net::ERR_INVALID_RESPONSE,
                 /*expected_outer_response_error_code=*/std::nullopt,
                 /*expected_inner_response_code=*/std::nullopt);
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesMultipleRequests) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    TestOhttpClient client_a;
    client_a.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"Response a",
        /*expected_headers=*/{{"cache-control", "s-maxage=60"}});
    network::mojom::ObliviousHttpRequestPtr request_a = CreateRequest();
    TestOhttpClient client_b;
    client_b.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"Response b",
        /*expected_headers=*/{{"cache-control", "s-maxage=600"}});
    network::mojom::ObliviousHttpRequestPtr request_b = CreateRequest();
    request_b->relay_url = GURL("https://another.relay.test");

    handler->StartRequest(std::move(request_a), client_a.CreatePendingRemote());
    handler->StartRequest(std::move(request_b), client_b.CreatePendingRemote());

    RespondToPendingRequest("Response b", {{"cache-control", "s-maxage=600"}},
                            GURL("https://another.relay.test"));
    client_b.WaitForCall();

    RespondToPendingRequest("Response a", {{"cache-control", "s-maxage=60"}});
    client_a.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, PadsUpToNextPowerOfTwo) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"response body",
        /*expected_headers=*/{});
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->padding_params =
        network::mojom::ObliviousHttpPaddingParameters::New(
            /*add_exponential_pad=*/false, /*exponential_mean=*/0,
            /*pad_to_next_power_of_two=*/true);

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    const network::ResourceRequest* pending_request;
    ASSERT_TRUE(loader_factory()->IsPending(kRelayURL, &pending_request));
    ASSERT_TRUE(pending_request->request_body);
    ASSERT_EQ(1u, pending_request->request_body->elements()->size());

    std::string body = std::string(pending_request->request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());
    std::string plain_text_body = DecryptRequest(body);

    EXPECT_EQ(256u, plain_text_body.size());
    EXPECT_EQ(std::string(56, '\0'), plain_text_body.substr(200));
  }
}

TEST_F(TestObliviousHttpRequestHandler, DoesntPadsIfAlreadyPowerOfTwo) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"response body",
        /*expected_headers=*/{});
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->padding_params =
        network::mojom::ObliviousHttpPaddingParameters::New(
            /*add_exponential_pad=*/false, /*exponential_mean=*/0,
            /*pad_to_next_power_of_two=*/false);
    request->request_body = network::mojom::ObliviousHttpRequestBody::New(
        /*content=*/std::string(380, ' '),
        /*content_type=*/"application/testdata");

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    const network::ResourceRequest* pending_request;
    ASSERT_TRUE(loader_factory()->IsPending(kRelayURL, &pending_request));
    ASSERT_TRUE(pending_request->request_body);
    ASSERT_EQ(1u, pending_request->request_body->elements()->size());

    std::string body = std::string(pending_request->request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());
    std::string plain_text_body = DecryptRequest(body);

    EXPECT_EQ(512u, plain_text_body.size());
    EXPECT_EQ(std::string(380, ' '), plain_text_body.substr(512 - 380));
  }
}

TEST_F(TestObliviousHttpRequestHandler, PadsExponentiallyRandomly) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  const size_t kNumRuns = 100;
  double accum_size = 0;
  double accum_size_squared = 0;
  for (size_t i = 0; i < kNumRuns; i++) {
    {
      TestOhttpClient client;
      client.SetExpectedInnerResponse(
          /*expected_inner_response_code=*/net::HTTP_OK,
          /*expected_body=*/"response body",
          /*expected_headers=*/{});
      network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
      request->padding_params =
          network::mojom::ObliviousHttpPaddingParameters::New(
              /*add_exponential_pad=*/true, /*exponential_mean=*/10,
              /*pad_to_next_power_of_two=*/false);

      handler->StartRequest(std::move(request), client.CreatePendingRemote());
      const network::ResourceRequest* pending_request;
      ASSERT_TRUE(loader_factory()->IsPending(kRelayURL, &pending_request));
      ASSERT_TRUE(pending_request->request_body);
      ASSERT_EQ(1u, pending_request->request_body->elements()->size());

      std::string body = std::string(pending_request->request_body->elements()
                                         ->at(0)
                                         .As<network::DataElementBytes>()
                                         .AsStringPiece());
      accum_size += body.size();
      accum_size_squared += body.size() * body.size();
    }
  }

  double mean = accum_size / kNumRuns;
  double variance = accum_size_squared / kNumRuns - mean * mean;
  // True variance should be 100, but we're not running enough iterations for
  // the estimate to converge. This at least excludes the case where the padding
  // is constant.
  EXPECT_LT(200l, mean);
  EXPECT_GT(210l, mean);
  EXPECT_LT(16l, variance);
  EXPECT_GT(256l, variance);
}

TEST_F(TestObliviousHttpRequestHandler,
       PadsBothExponentiallyRandomlyAndPowerOfTwo) {
  std::unique_ptr<network::ObliviousHttpRequestHandler> handler =
      CreateHandler();
  base::flat_set<size_t> sizes_seen;
  while (sizes_seen.size() < 2) {
    TestOhttpClient client;
    client.SetExpectedInnerResponse(
        /*expected_inner_response_code=*/net::HTTP_OK,
        /*expected_body=*/"response body",
        /*expected_headers=*/{});
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->padding_params =
        network::mojom::ObliviousHttpPaddingParameters::New(
            /*add_exponential_pad=*/true, /*exponential_mean=*/10,
            /*pad_to_next_power_of_two=*/true);
    // Set message size to 246 bytes plus an average of 10 bytes padding.
    request->request_body = network::mojom::ObliviousHttpRequestBody::New(
        /*content=*/std::string(114, ' '),
        /*content_type=*/"application/testdata");

    handler->StartRequest(std::move(request), client.CreatePendingRemote());
    const network::ResourceRequest* pending_request;
    ASSERT_TRUE(loader_factory()->IsPending(kRelayURL, &pending_request));
    ASSERT_TRUE(pending_request->request_body);
    ASSERT_EQ(1u, pending_request->request_body->elements()->size());

    std::string body = std::string(pending_request->request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());

    std::string plain_text_body = DecryptRequest(body);
    size_t body_size = plain_text_body.size();
    sizes_seen.insert(body_size);

    // body_size must be a power of 2.
    // That means all of the bits except the high bit are 0.
    size_t cur = body_size;
    bool has_bad_bits = false;
    while (cur > 1) {
      has_bad_bits |= (cur & 0x1) != 0u;
      cur >>= 1;
    }
    EXPECT_FALSE(has_bad_bits) << "Got non-power of 2 body size " << body_size;
  }
}
