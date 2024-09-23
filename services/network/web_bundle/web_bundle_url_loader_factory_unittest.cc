// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/web_package/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/web_bundle/web_bundle_memory_quota_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const char kInitiatorUrl[] = "https://example.com/";
const char kBundleUrl[] = "https://example.com/bundle.wbn";
const char kBundleRequestId[] = "bundle-devtools-request-id";
const char kResourceUrl[] = "https://example.com/";
const char kResourceUrl2[] = "https://example.com/another";
const char kResourceUrl3[] = "https://example.com/yetanother";
const char kInvalidResourceUrl[] = "ftp://foo";
const char kResourceRequestId[] = "resource-1-devtools-request-id";
const char kResourceRequestId2[] = "resource-2-devtools-request-id";
const char kResourceRequestId3[] = "resource-3-devtools-request-id";

// Cross origin resources
const char kCrossOriginJsonUrl[] = "https://other.com/resource.json";
const char kCrossOriginJsUrl[] = "https://other.com/resource.js";

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Pointee;

std::vector<uint8_t> CreateSmallBundle() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  return builder.CreateBundle();
}

std::vector<uint8_t> CreateLargeBundle() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  builder.AddExchange(kResourceUrl2,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      std::string(10000, 'a'));
  builder.AddExchange(kResourceUrl3,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  return builder.CreateBundle();
}

std::vector<uint8_t> CreateCrossOriginBundle() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      kCrossOriginJsonUrl,
      {{":status", "200"}, {"content-type", "application/json"}},
      "{ secret: 1 }");
  builder.AddExchange(kCrossOriginJsUrl,
                      {{":status", "200"}, {"content-type", "application/js"}},
                      "const not_secret = 1;");
  return builder.CreateBundle();
}

class TestWebBundleHandle : public mojom::WebBundleHandle {
 public:
  explicit TestWebBundleHandle(
      mojo::PendingReceiver<mojom::WebBundleHandle> receiver)
      : receiver_(this, std::move(receiver)) {}

  const std::optional<std::pair<mojom::WebBundleErrorType, std::string>>&
  last_bundle_error() const {
    return last_bundle_error_;
  }

  void RunUntilBundleError() {
    if (last_bundle_error_.has_value())
      return;
    base::RunLoop run_loop;
    quit_closure_for_bundle_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // mojom::WebBundleHandle
  void Clone(mojo::PendingReceiver<mojom::WebBundleHandle> receiver) override {
    NOTREACHED_IN_MIGRATION();
  }

  void OnWebBundleError(mojom::WebBundleErrorType type,
                        const std::string& message) override {
    last_bundle_error_ = std::make_pair(type, message);
    if (quit_closure_for_bundle_error_)
      std::move(quit_closure_for_bundle_error_).Run();
  }

  void OnWebBundleLoadFinished(bool success) override {}

 private:
  mojo::Receiver<mojom::WebBundleHandle> receiver_;
  std::optional<std::pair<mojom::WebBundleErrorType, std::string>>
      last_bundle_error_;
  base::OnceClosure quit_closure_for_bundle_error_;
};

class MockMemoryQuotaConsumer : public WebBundleMemoryQuotaConsumer {
 public:
  MockMemoryQuotaConsumer() = default;
  ~MockMemoryQuotaConsumer() override = default;

  bool AllocateMemory(uint64_t num_bytes) override { return true; }
};

class BadMessageTestHelper {
 public:
  BadMessageTestHelper()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BadMessageTestHelper::OnBadMessage, base::Unretained(this)));
  }
  BadMessageTestHelper(const BadMessageTestHelper&) = delete;
  BadMessageTestHelper& operator=(const BadMessageTestHelper&) = delete;

  ~BadMessageTestHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  const std::vector<std::string>& bad_message_reports() const {
    return bad_message_reports_;
  }

 private:
  void OnBadMessage(const std::string& reason) {
    bad_message_reports_.push_back(reason);
  }

  std::vector<std::string> bad_message_reports_;

  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;
};

}  // namespace

class WebBundleURLLoaderFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(CreateDataPipe(nullptr, bundle_data_destination_, consumer),
              MOJO_RESULT_OK);
    mojo::Remote<mojom::WebBundleHandle> handle;
    handle_ = std::make_unique<TestWebBundleHandle>(
        handle.BindNewPipeAndPassReceiver());
    devtools_observer_ = std::make_unique<MockDevToolsObserver>();
    const ResourceRequest::WebBundleTokenParams create_params(
        GURL(kBundleUrl), {} /* token */, {} /* handle */);
    factory_ = std::make_unique<WebBundleURLLoaderFactory>(
        GURL(kBundleUrl), create_params, std::move(handle),
        std::make_unique<MockMemoryQuotaConsumer>(), devtools_observer_->Bind(),
        kBundleRequestId, CrossOriginEmbedderPolicy(),
        nullptr /* coep_reporter */);
    factory_->SetBundleStream(std::move(consumer));
  }

  void WriteBundle(base::span<const uint8_t> data) {
    mojo::BlockingCopyFromString(
        std::string(reinterpret_cast<const char*>(data.data()), data.size()),
        bundle_data_destination_);
  }

  void FinishWritingBundle() { bundle_data_destination_.reset(); }

  struct StartRequestResult {
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
  };

  network::ResourceRequest CreateRequest(
      const GURL& url,
      const std::string& devtools_request_id) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin::Create(GURL(kInitiatorUrl));
    request.web_bundle_token_params = ResourceRequest::WebBundleTokenParams();
    request.web_bundle_token_params->bundle_url = GURL(kBundleUrl);
    request.devtools_request_id = devtools_request_id;
    return request;
  }

  StartRequestResult StartRequest(const ResourceRequest& request) {
    StartRequestResult result;
    result.client = std::make_unique<network::TestURLLoaderClient>();
    factory_->StartLoader(WebBundleURLLoaderFactory::CreateURLLoader(
        result.loader.BindNewPipeAndPassReceiver(), request,
        result.client->CreateRemote(),
        mojo::Remote<mojom::TrustedHeaderClient>(), base::Time::Now(),
        base::TimeTicks::Now(), base::DoNothing()));
    return result;
  }

  StartRequestResult StartRequest(const GURL& url,
                                  const std::string& devtools_request_id) {
    return StartRequest(CreateRequest(url, devtools_request_id));
  }

  void RunUntilBundleError() { handle_->RunUntilBundleError(); }

  const std::optional<std::pair<mojom::WebBundleErrorType, std::string>>&
  last_bundle_error() const {
    return handle_->last_bundle_error();
  }

 protected:
  std::unique_ptr<MockDevToolsObserver> devtools_observer_;
  std::unique_ptr<WebBundleURLLoaderFactory> factory_;

 private:
  std::unique_ptr<TestWebBundleHandle> handle_;
  mojo::ScopedDataPipeProducerHandle bundle_data_destination_;
  base::test::TaskEnvironment task_environment;
};

TEST_F(WebBundleURLLoaderFactoryTest, Basic) {
  base::HistogramTester histogram_tester;
  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadata(kBundleRequestId,
                                             ElementsAre(GURL(kResourceUrl))));
  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleInnerResponse(
                  kResourceRequestId, GURL(kResourceUrl),
                  Optional(std::string(kBundleRequestId))));

  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
  EXPECT_FALSE(last_bundle_error().has_value());
  EXPECT_TRUE(request.client->response_head()->is_web_bundle_inner_response);
  std::string body;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      request.client->response_body_release(), &body));
  EXPECT_EQ("body", body);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ResourceCount", 1,
                                      1);
}

TEST_F(WebBundleURLLoaderFactoryTest, MetadataParseError) {
  base::HistogramTester histogram_tester;
  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);

  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle[4] ^= 1;  // Mutate magic bytes.
  WriteBundle(bundle);
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadataError(kBundleRequestId,
                                                  Eq("Wrong magic bytes.")));
  EXPECT_CALL(*devtools_observer_, OnSubresourceWebBundleInnerResponse(_, _, _))
      .Times(0);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kMetadataParseError);
  EXPECT_EQ(last_bundle_error()->second, "Wrong magic bytes.");

  // Requests made after metadata parse error should also fail.
  auto request2 = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  request2.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request2.client->completion_status().error_code);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
          kMetadataParseError,
      1);
}

TEST_F(WebBundleURLLoaderFactoryTest, MetadataWithInvalidExchangeUrl) {
  base::HistogramTester histogram_tester;
  auto request = StartRequest(GURL(kInvalidResourceUrl), kResourceRequestId);

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kInvalidResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadataError(
                  kBundleRequestId, Eq("Exchange URL is not valid.")));
  EXPECT_CALL(*devtools_observer_, OnSubresourceWebBundleInnerResponse(_, _, _))
      .Times(0);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kMetadataParseError);
  EXPECT_EQ(last_bundle_error()->second, "Exchange URL is not valid.");

  // Requests made after metadata parse error should also fail.
  auto request2 = StartRequest(GURL(kInvalidResourceUrl), kResourceRequestId);
  request2.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request2.client->completion_status().error_code);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
          kMetadataParseError,
      1);
}

TEST_F(WebBundleURLLoaderFactoryTest, ResponseParseError) {
  web_package::WebBundleBuilder builder;
  // An invalid response.
  builder.AddExchange(kResourceUrl, {{":status", "0"}}, "body");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadata(kBundleRequestId,
                                             ElementsAre(GURL(kResourceUrl))));
  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleInnerResponseError(
                  kResourceRequestId, GURL(kResourceUrl),
                  Eq(":status must be 3 ASCII decimal digits."),
                  Optional(std::string(kBundleRequestId))));

  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second,
            ":status must be 3 ASCII decimal digits.");
}

TEST_F(WebBundleURLLoaderFactoryTest, ResourceNotFoundInBundle) {
  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadata(kBundleRequestId,
                                             ElementsAre(GURL(kResourceUrl))));
  EXPECT_CALL(*devtools_observer_, OnSubresourceWebBundleInnerResponse(_, _, _))
      .Times(0);

  auto request = StartRequest(GURL("https://example.com/no-such-resource"),
                              kResourceRequestId);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kResourceNotFound);
  EXPECT_EQ(
      last_bundle_error()->second,
      "https://example.com/no-such-resource is not found in the WebBundle.");
}

TEST_F(WebBundleURLLoaderFactoryTest, RedirectResponseIsNotAllowed) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "301"}, {"location", kResourceUrl2}}, "");
  builder.AddExchange(kResourceUrl2,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleMetadata(
                  kBundleRequestId,
                  ElementsAre(GURL(kResourceUrl), GURL(kResourceUrl2))));
  EXPECT_CALL(*devtools_observer_,
              OnSubresourceWebBundleInnerResponse(
                  kResourceRequestId, GURL(kResourceUrl),
                  Optional(std::string(kBundleRequestId))));

  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second, "Invalid response code 301");
}

TEST_F(WebBundleURLLoaderFactoryTest, StartRequestBeforeReadingBundle) {
  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
}

TEST_F(WebBundleURLLoaderFactoryTest, MultipleRequests) {
  base::HistogramTester histogram_tester;
  auto request1 = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  auto request2 = StartRequest(GURL(kResourceUrl2), kResourceRequestId2);

  std::vector<uint8_t> bundle = CreateLargeBundle();
  // Write the first 10kB of the bundle in which the bundle's metadata and the
  // response for kResourceUrl are included.
  ASSERT_GT(bundle.size(), 10000u);
  WriteBundle(base::make_span(bundle).first(10000u));
  request1.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request1.client->completion_status().error_code);
  EXPECT_FALSE(request2.client->has_received_completion());

  // Write the rest of the data.
  WriteBundle(base::make_span(bundle).subspan(10000u));
  FinishWritingBundle();
  request2.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request2.client->completion_status().error_code);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ResourceCount", 3,
                                      1);
}

TEST_F(WebBundleURLLoaderFactoryTest, CancelRequest) {
  auto request_to_complete1 =
      StartRequest(GURL(kResourceUrl), kResourceRequestId);
  auto request_to_complete2 =
      StartRequest(GURL(kResourceUrl2), kResourceRequestId2);
  auto request_to_cancel1 =
      StartRequest(GURL(kResourceUrl), kResourceRequestId);
  auto request_to_cancel2 =
      StartRequest(GURL(kResourceUrl2), kResourceRequestId2);
  auto request_to_cancel3 =
      StartRequest(GURL(kResourceUrl3), kResourceRequestId3);

  // Cancel request before getting metadata.
  request_to_cancel1.loader.reset();

  std::vector<uint8_t> bundle = CreateLargeBundle();
  // Write the first 10kB of the bundle in which the bundle's metadata, response
  // for kResourceUrl, and response header for kResourceUrl2 are included.
  ASSERT_GT(bundle.size(), 10000U);
  WriteBundle(base::make_span(bundle).first(10000u));

  // This makes sure the bytes written above are consumed by WebBundle parser.
  request_to_complete1.client->RunUntilComplete();

  // Cancel request after reading response header, but before reading body.
  request_to_cancel2.loader.reset();

  // Cancel request after getting metadata, but before reading response header.
  request_to_cancel3.loader.reset();

  // Write the rest of the data.
  WriteBundle(base::make_span(bundle).subspan(10000));
  FinishWritingBundle();
  request_to_complete2.client->RunUntilComplete();
  EXPECT_EQ(net::OK,
            request_to_complete2.client->completion_status().error_code);
}

TEST_F(WebBundleURLLoaderFactoryTest,
       FactoryDestructionCancelsInflightRequests) {
  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);

  factory_ = nullptr;

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();
  request.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, request.client->completion_status().error_code);
}

TEST_F(WebBundleURLLoaderFactoryTest, TruncatedBundle) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  // Truncate in the middle of responses section.
  bundle.resize(bundle.size() - 10);
  WriteBundle(std::move(bundle));
  FinishWritingBundle();

  auto request = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            mojom::WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second, "Error reading response header.");
}

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginJson) {
  WriteBundle(CreateCrossOriginBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL(kCrossOriginJsonUrl), kResourceRequestId);
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
  EXPECT_FALSE(last_bundle_error().has_value());
  std::string body;
  ASSERT_TRUE(mojo::BlockingCopyToString(
      request.client->response_body_release(), &body));
  EXPECT_TRUE(body.empty())
      << "body should be empty because JSON is a ORB-protected resource";
}

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginJs) {
  WriteBundle(CreateCrossOriginBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL(kCrossOriginJsUrl), kResourceRequestId);
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
  EXPECT_FALSE(last_bundle_error().has_value());
  std::string body;
  ASSERT_TRUE(mojo::BlockingCopyToString(
      request.client->response_body_release(), &body));
  EXPECT_EQ("const not_secret = 1;", body)
      << "body should be valid one because JS is not a ORB protected resource";
}

TEST_F(WebBundleURLLoaderFactoryTest, WrongBundleURL) {
  BadMessageTestHelper bad_message_helper;

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  network::ResourceRequest url_request =
      CreateRequest(GURL(kResourceUrl), kResourceRequestId);
  url_request.web_bundle_token_params->bundle_url =
      GURL("https://modified-bundle-url.example.com/");
  auto request = StartRequest(url_request);
  request.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            request.client->completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "WebBundleURLLoaderFactory: Bundle URL does not match"));
}

}  // namespace network
