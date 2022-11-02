// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/binary_http/binary_http_message.h"
#include "net/third_party/quiche/src/quiche/common/quiche_data_writer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/oblivious_http_request_handler.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace {

const char kRelayURL[] = "https://relay.test:13/";
const char kResourceURL[] = "https://resource.test:37/path";

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

class TestOhttpNetworkContext : public network::TestNetworkContext {
 public:
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      network::mojom::URLLoaderFactoryParamsPtr params) override {
    EXPECT_EQ(network::mojom::kBrowserProcessId, params->process_id);
    EXPECT_FALSE(params->is_corb_enabled);
    EXPECT_TRUE(params->is_trusted);

    factory_reciever_ =
        std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
            &loader_factory_, std::move(receiver));
  }

  network::TestURLLoaderFactory* loader_factory() { return &loader_factory_; }

 private:
  network::TestURLLoaderFactory loader_factory_;
  std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderFactory>>
      factory_reciever_;
};

class TestOhttpClient : public network::mojom::ObliviousHttpClient {
 public:
  TestOhttpClient(absl::optional<std::string> expected_body,
                  int expected_status)
      : expected_body_(std::move(expected_body)),
        expected_status_(expected_status),
        receiver_(this) {}

  mojo::PendingRemote<network::mojom::ObliviousHttpClient>
  CreatePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnCompleted(const absl::optional<std::string>& response,
                   int net_error) override {
    EXPECT_EQ(expected_body_, response);
    EXPECT_EQ(expected_status_, net_error);
    run_loop_.Quit();
  }

  void WaitForCall() { run_loop_.Run(); }

 private:
  const absl::optional<std::string> expected_body_;
  const int expected_status_;
  mojo::Receiver<network::mojom::ObliviousHttpClient> receiver_;
  base::RunLoop run_loop_;
};

}  // namespace

class TestObliviousHttpRequestHandler : public testing::Test {
 public:
  TestObliviousHttpRequestHandler()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  TestOhttpNetworkContext* network_context() { return &network_context_; }
  network::TestURLLoaderFactory* loader_factory() {
    return network_context()->loader_factory();
  }

  void AddResponse(std::string body, GURL relay_url = GURL(kRelayURL)) {
    quiche::BinaryHttpResponse response(net::HTTP_OK);
    response.set_body(std::move(body));
    loader_factory()->AddResponse(
        /*url=*/relay_url,
        /*head=*/network::CreateURLResponseHead(net::HTTP_OK),
        /*content=*/response.Serialize().value(),
        /*status=*/network::URLLoaderCompletionStatus());
  }

  network::mojom::ObliviousHttpRequestPtr CreateRequest() {
    network::mojom::ObliviousHttpRequestPtr request =
        network::mojom::ObliviousHttpRequest::New();
    request->relay_url = GURL(kRelayURL);
    request->resource_url = GURL(kResourceURL);
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    request->request_body = network::mojom::ObliviousHttpRequestBody::New(
        /*content=*/"test data", /*content_type=*/"application/testdata");
    return request;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestOhttpNetworkContext network_context_;
};

TEST_F(TestObliviousHttpRequestHandler, TestDisconnect) {
  network::ObliviousHttpRequestHandler handler(network_context());
  AddResponse("");
  {
    TestOhttpClient client("", net::OK);
    handler.StartRequest(CreateRequest(), client.CreatePendingRemote());
  }

  {
    TestOhttpClient client("", net::OK);
    handler.StartRequest(CreateRequest(), client.CreatePendingRemote());
    client.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, TestInvalidArguments) {
  network::ObliviousHttpRequestHandler handler(network_context());
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request =
        network::mojom::ObliviousHttpRequest::New();

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Relay URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->relay_url = GURL();

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Relay URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_URL);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->resource_url = GURL();

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Resource URL", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->traffic_annotation = net::MutableNetworkTrafficAnnotationTag();

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Invalid OHTTP Traffic Annotation", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->request_body->content = std::string(10 * 1024 + 1, ' ');

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Request body too large", obs.WaitForBadMessage());
  }
  {
    mojo::FakeMessageDispatchContext context;
    mojo::test::BadMessageObserver obs;
    TestOhttpClient client(absl::nullopt, net::ERR_INVALID_ARGUMENT);
    network::mojom::ObliviousHttpRequestPtr request = CreateRequest();
    request->request_body->content_type = std::string(257, ' ');

    handler.StartRequest(std::move(request), client.CreatePendingRemote());
    EXPECT_EQ("Content-Type too large", obs.WaitForBadMessage());
  }
}

TEST_F(TestObliviousHttpRequestHandler, TestRequestFormat) {
  network::ObliviousHttpRequestHandler handler(network_context());
  {
    TestOhttpClient client("response body", net::OK);

    handler.StartRequest(CreateRequest(), client.CreatePendingRemote());
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
    // TODO(behamilton): decryption
    std::string plain_text_body = body;
    auto maybe_request = quiche::BinaryHttpRequest::Create(body);
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
    AddResponse("response body");
    client.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesOuterHttpError) {
  network::ObliviousHttpRequestHandler handler(network_context());
  {
    loader_factory()->AddResponse(kRelayURL, "", net::HTTP_NOT_FOUND);
    TestOhttpClient client(absl::nullopt, net::ERR_HTTP_RESPONSE_CODE_FAILURE);

    handler.StartRequest(CreateRequest(), client.CreatePendingRemote());
    client.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesInnerHttpError) {
  network::ObliviousHttpRequestHandler handler(network_context());
  {
    quiche::BinaryHttpResponse response(net::HTTP_NOT_FOUND);
    loader_factory()->AddResponse(kRelayURL, response.Serialize().value(),
                                  net::HTTP_OK);
    TestOhttpClient client(absl::nullopt, net::ERR_HTTP_RESPONSE_CODE_FAILURE);

    handler.StartRequest(CreateRequest(), client.CreatePendingRemote());
    client.WaitForCall();
  }
}

TEST_F(TestObliviousHttpRequestHandler, HandlesMultipleRequests) {
  network::ObliviousHttpRequestHandler handler(network_context());
  {
    TestOhttpClient client_a("", net::OK);
    network::mojom::ObliviousHttpRequestPtr request_a = CreateRequest();
    TestOhttpClient client_b("", net::OK);
    network::mojom::ObliviousHttpRequestPtr request_b = CreateRequest();
    request_b->relay_url = GURL("https://another.relay.test");

    handler.StartRequest(std::move(request_a), client_a.CreatePendingRemote());
    handler.StartRequest(std::move(request_b), client_b.CreatePendingRemote());

    AddResponse("", GURL("https://another.relay.test"));
    client_b.WaitForCall();

    AddResponse("");
    client_a.WaitForCall();
  }
}