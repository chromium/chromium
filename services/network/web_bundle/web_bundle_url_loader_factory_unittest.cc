// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/web_package/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/web_bundle/web_bundle_memory_quota_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const char kInitiatorUrl[] = "https://example.com/";
const char kBundleUrl[] = "https://example.com/bundle.wbn";
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

  void RunUntilBundleLoadFinished() {
    if (is_load_finished_) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_for_load_finished_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // mojom::WebBundleHandle
  void Clone(mojo::PendingReceiver<mojom::WebBundleHandle> receiver) override {
    NOTREACHED();
  }

  void OnWebBundleError(mojom::WebBundleErrorType type,
                        const std::string& message) override {
    last_bundle_error_ = std::make_pair(type, message);
    if (quit_closure_for_bundle_error_)
      std::move(quit_closure_for_bundle_error_).Run();
  }

  void OnWebBundleLoadFinished(bool success) override {
    is_load_finished_ = true;
    if (quit_closure_for_load_finished_) {
      std::move(quit_closure_for_load_finished_).Run();
    }
  }

 private:
  mojo::Receiver<mojom::WebBundleHandle> receiver_;
  std::optional<std::pair<mojom::WebBundleErrorType, std::string>>
      last_bundle_error_;
  base::OnceClosure quit_closure_for_bundle_error_;
  bool is_load_finished_ = false;
  base::OnceClosure quit_closure_for_load_finished_;
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
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {}
  BadMessageTestHelper(const BadMessageTestHelper&) = delete;
  BadMessageTestHelper& operator=(const BadMessageTestHelper&) = delete;

  ~BadMessageTestHelper() = default;

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
  mojo::ScopedDefaultProcessErrorHandlerForTesting error_handler_{
      base::BindRepeating(&BadMessageTestHelper::OnBadMessage,
                          base::Unretained(this))};
};

// Test implementation of mojom::CrossOriginEmbedderPolicyReporter that records
// reported blocked URLs from CORP violations.
class TestCoepReporter : public mojom::CrossOriginEmbedderPolicyReporter {
 public:
  TestCoepReporter() = default;
  ~TestCoepReporter() override = default;

  void QueueCorpViolationReport(const GURL& blocked_url,
                                mojom::RequestDestination destination,
                                bool report_only) override {
    reports_.push_back(blocked_url);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void Clone(mojo::PendingReceiver<mojom::CrossOriginEmbedderPolicyReporter>
                 receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  mojo::PendingRemote<mojom::CrossOriginEmbedderPolicyReporter>
  CreatePendingRemote() {
    mojo::PendingRemote<mojom::CrossOriginEmbedderPolicyReporter> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void RunUntilReport() {
    if (!reports_.empty()) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const std::vector<GURL>& reports() const { return reports_; }

 private:
  std::vector<GURL> reports_;
  base::OnceClosure quit_closure_;
  mojo::ReceiverSet<mojom::CrossOriginEmbedderPolicyReporter> receivers_;
};

}  // namespace

class WebBundleURLLoaderFactoryTest : public ::testing::Test {
 public:
  WebBundleURLLoaderFactoryTest()
      : WebBundleURLLoaderFactoryTest(CrossOriginEmbedderPolicy()) {}

  explicit WebBundleURLLoaderFactoryTest(
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy)
      : cross_origin_embedder_policy_(cross_origin_embedder_policy) {}

  void SetUp() override {
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(CreateDataPipe(nullptr, bundle_data_destination_, consumer),
              MOJO_RESULT_OK);
    mojo::Remote<mojom::WebBundleHandle> handle;
    handle_ = std::make_unique<TestWebBundleHandle>(
        handle.BindNewPipeAndPassReceiver());

    // In production, CorsURLLoaderFactory owns the COEP reporter remote and
    // clones it for WebBundleURLLoaderFactory. We mirror that ownership here.
    mojo::PendingRemote<mojom::CrossOriginEmbedderPolicyReporter> cloned_remote;
    original_coep_reporter_.Bind(coep_reporter_.CreatePendingRemote());
    original_coep_reporter_->Clone(
        cloned_remote.InitWithNewPipeAndPassReceiver());

    const ResourceRequest::WebBundleTokenParams create_params(
        GURL(kBundleUrl), {} /* token */, {} /* handle */);
    factory_ = std::make_unique<WebBundleURLLoaderFactory>(
        GURL(kBundleUrl), create_params, std::move(handle),
        std::make_unique<MockMemoryQuotaConsumer>(),
        cross_origin_embedder_policy_, std::move(cloned_remote));
    factory_->SetBundleStream(std::move(consumer));
  }

  void ResetOriginalCoepReporter() { original_coep_reporter_.reset(); }

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

  void RunUntilBundleLoadFinished() { handle_->RunUntilBundleLoadFinished(); }

  const std::optional<std::pair<mojom::WebBundleErrorType, std::string>>&
  last_bundle_error() const {
    return handle_->last_bundle_error();
  }

  void RunUntilCoepReport() { coep_reporter_.RunUntilReport(); }

  const std::vector<GURL>& coep_reports() const {
    return coep_reporter_.reports();
  }

  void ExpectResponseBlockedByOrb(StartRequestResult& request) {
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    EXPECT_EQ(
        0u, request.client->completion_status().decoded_body_length.InBytes());
    ASSERT_TRUE(request.client->response_head());
    EXPECT_EQ(0, request.client->response_head()->content_length);
    ASSERT_TRUE(request.client->response_head()->headers);
    size_t iter = 0;
    std::string name;
    std::string value;
    EXPECT_FALSE(request.client->response_head()->headers->EnumerateHeaderLines(
        &iter, &name, &value));
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_TRUE(body.empty());
  }

 protected:
  std::unique_ptr<WebBundleURLLoaderFactory> factory_;

 private:
  base::test::TaskEnvironment task_environment;

  std::unique_ptr<TestWebBundleHandle> handle_;
  mojo::ScopedDataPipeProducerHandle bundle_data_destination_;

  const CrossOriginEmbedderPolicy cross_origin_embedder_policy_;
  mojo::Remote<mojom::CrossOriginEmbedderPolicyReporter>
      original_coep_reporter_;
  TestCoepReporter coep_reporter_;
};

TEST_F(WebBundleURLLoaderFactoryTest, Basic) {
  base::HistogramTester histogram_tester;
  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

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
  WriteBundle(base::span(bundle).first<10000>());
  request1.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request1.client->completion_status().error_code);
  EXPECT_FALSE(request2.client->has_received_completion());

  // Write the rest of the data.
  WriteBundle(base::span(bundle).subspan<10000>());
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
  WriteBundle(base::span(bundle).first<10000>());

  // This makes sure the bytes written above are consumed by WebBundle parser.
  request_to_complete1.client->RunUntilComplete();

  // Cancel request after reading response header, but before reading body.
  request_to_cancel2.loader.reset();

  // Cancel request after getting metadata, but before reading response header.
  request_to_cancel3.loader.reset();

  // Write the rest of the data.
  WriteBundle(base::span(bundle).subspan<10000>());
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

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginSniffedContentBlockedByOrb) {
  const GURL kOctetStreamJsonUrl("https://other.test/octet-stream-json");
  const GURL kJsWithJsonUrl("https://other.test/js-with-json");
  const GURL kJsWithParserBreakerUrl(
      "https://other.test/js-with-parser-breaker");
  const GURL kPngWithHtmlUrl("https://other.test/png-with-html");
  const GURL kOctetStreamXmlUrl("https://other.test/octet-stream-xml");

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kOctetStreamJsonUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-test-header", "value"}},
                      "{\"key\": 1}");
  builder.AddExchange(
      kJsWithJsonUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "{\"key\": 1}");
  builder.AddExchange(
      kJsWithParserBreakerUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      ")]}'\n{\"key\": 1}");
  builder.AddExchange(kPngWithHtmlUrl,
                      {{":status", "200"}, {"content-type", "image/png"}},
                      "<!DOCTYPE html><html><body>text</body></html>");
  builder.AddExchange(
      kOctetStreamXmlUrl,
      {{":status", "200"}, {"content-type", "application/octet-stream"}},
      "<?xml version=\"1.0\"?><root/>");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  for (const GURL& url :
       {kOctetStreamJsonUrl, kJsWithJsonUrl, kJsWithParserBreakerUrl,
        kPngWithHtmlUrl, kOctetStreamXmlUrl}) {
    SCOPED_TRACE(url.spec());
    auto request = StartRequest(url, kResourceRequestId);
    request.client->RunUntilComplete();

    EXPECT_FALSE(last_bundle_error().has_value());
    ExpectResponseBlockedByOrb(request);
  }
}

TEST_F(WebBundleURLLoaderFactoryTest,
       CrossOriginSniffedEmptyAndMediaAllowedByOrb) {
  const GURL kEmptyJsUrl("https://other.test/empty.js");
  const GURL kEmptyOctetStreamUrl("https://other.test/empty.bin");
  const GURL kOctetStreamPngUrl("https://other.test/image.bin");
  const GURL kOctetStreamWebmUrl("https://other.test/video.bin");
  const std::string kPngBody("\x89PNG\r\n\x1a\npayload", 15);
  const std::string kWebmBody("\x1a\x45\xdf\xa3payload", 11);

  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      kEmptyJsUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}}, "");
  builder.AddExchange(
      kEmptyOctetStreamUrl,
      {{":status", "200"}, {"content-type", "application/octet-stream"}}, "");
  builder.AddExchange(
      kOctetStreamPngUrl,
      {{":status", "200"}, {"content-type", "application/octet-stream"}},
      kPngBody);
  builder.AddExchange(
      kOctetStreamWebmUrl,
      {{":status", "200"}, {"content-type", "application/octet-stream"}},
      kWebmBody);
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  {
    auto request = StartRequest(kEmptyJsUrl, kResourceRequestId);
    request.client->RunUntilComplete();
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    ASSERT_TRUE(request.client->response_head()->headers);
    EXPECT_TRUE(request.client->response_head()->headers->HasHeaderValue(
        "content-type", "application/javascript"));
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_TRUE(body.empty());
  }

  {
    auto request = StartRequest(kEmptyOctetStreamUrl, kResourceRequestId2);
    request.client->RunUntilComplete();
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    ASSERT_TRUE(request.client->response_head()->headers);
    EXPECT_TRUE(request.client->response_head()->headers->HasHeaderValue(
        "content-type", "application/octet-stream"));
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_TRUE(body.empty());
  }

  {
    auto request = StartRequest(kOctetStreamPngUrl, kResourceRequestId3);
    request.client->RunUntilComplete();
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_EQ(kPngBody, body);
  }

  {
    auto request = StartRequest(kOctetStreamWebmUrl, kResourceRequestId);
    request.client->RunUntilComplete();
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_EQ(kWebmBody, body);
  }
}

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginSniffedLargePayload) {
  const GURL kLargeJsonUrl("https://other.test/large-json");
  const GURL kLargeJsUrl("https://other.test/large-js");
  const std::string kLargeJsonBody =
      "{\"data\": \"" + std::string(2000, 'a') + "\"}";
  const std::string kLargeJsBody =
      "const data = \"" + std::string(2000, 'a') + "\";";

  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      kLargeJsonUrl,
      {{":status", "200"}, {"content-type", "application/octet-stream"}},
      kLargeJsonBody);
  builder.AddExchange(
      kLargeJsUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      kLargeJsBody);
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  {
    auto request = StartRequest(kLargeJsonUrl, kResourceRequestId);
    request.client->RunUntilComplete();
    ExpectResponseBlockedByOrb(request);
  }

  {
    auto request = StartRequest(kLargeJsUrl, kResourceRequestId2);
    request.client->RunUntilComplete();
    EXPECT_EQ(net::OK, request.client->completion_status().error_code);
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(
        request.client->response_body_release(), &body));
    EXPECT_EQ(kLargeJsBody, body);
  }
}

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginSniffedStreamingBundle) {
  const GURL kStreamingJsonUrl("https://other.test/streaming-json");
  const GURL kStreamingJsUrl("https://other.test/streaming-js");
  const std::string kLargeJsonBody =
      "{\"data\": \"" + std::string(10000, 'a') + "\"}";
  const std::string kLargeJsBody =
      "const data = \"" + std::string(10000, 'a') + "\";";

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  // Pad response headers (> 3072 bytes) while staying within
  // INITIAL_BUFFER_SIZE_FOR_RESPONSE (4096 bytes), so ResponseParser finishes
  // parsing the header in its initial 4096-byte read while fewer than
  // net::kMaxBytesToSniff (1024) payload bytes are buffered.
  builder.AddExchange(kStreamingJsonUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-padding", std::string(4000, 'x')}},
                      kLargeJsonBody);
  builder.AddExchange(kStreamingJsUrl,
                      {{":status", "200"},
                       {"content-type", "application/javascript"},
                       {"x-padding", std::string(4000, 'x')}},
                      kLargeJsBody);
  std::vector<uint8_t> bundle = builder.CreateBundle();

  auto request1 = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  auto json_request = StartRequest(kStreamingJsonUrl, kResourceRequestId2);
  auto js_request = StartRequest(kStreamingJsUrl, kResourceRequestId3);

  // Write the first 4800 bytes: enough for metadata, kResourceUrl, and
  // ResponseParser's 4096-byte header read for kStreamingJsonUrl, so
  // SendResponseToLoader queues OnDataAvailableForOrb in pending_reads_, while
  // still short of kStreamingJsonUrl's first net::kMaxBytesToSniff body bytes.
  ASSERT_GT(bundle.size(), 5800u);
  WriteBundle(base::span(bundle).first<4800>());
  request1.client->RunUntilComplete();
  EXPECT_EQ(net::OK, request1.client->completion_status().error_code);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory_->PendingReadCountForTesting() == 2u; }));

  // Response must not be delivered while OnDataAvailableForOrb is waiting in
  // BundleDataSource::pending_reads_.
  EXPECT_FALSE(json_request.client->has_received_response());
  EXPECT_FALSE(js_request.client->has_received_response());

  // Stream another 1000 bytes (total 5800 bytes), crossing the 1024-byte sniff
  // threshold for kStreamingJsonUrl before the rest of its 10KB body arrives.
  WriteBundle(base::span(bundle).subspan<4800, 1000>());
  json_request.client->RunUntilComplete();
  ExpectResponseBlockedByOrb(json_request);
  EXPECT_FALSE(js_request.client->has_received_response());

  WriteBundle(base::span(bundle).subspan<5800>());
  FinishWritingBundle();

  js_request.client->RunUntilComplete();
  EXPECT_EQ(net::OK, js_request.client->completion_status().error_code);
  std::string js_body;
  ASSERT_TRUE(mojo::BlockingCopyToString(
      js_request.client->response_body_release(), &js_body));
  EXPECT_EQ(kLargeJsBody, js_body);
}

TEST_F(WebBundleURLLoaderFactoryTest,
       FactoryDestructionWhileOrbSniffingPending) {
  const GURL kStreamingJsonUrl("https://other.test/streaming-json");
  const GURL kStreamingJsUrl("https://other.test/streaming-js");

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kStreamingJsonUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-padding", std::string(4000, 'x')}},
                      "{\"data\": \"" + std::string(10000, 'a') + "\"}");
  builder.AddExchange(
      kStreamingJsUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "const data = \"" + std::string(10000, 'a') + "\";");
  std::vector<uint8_t> bundle = builder.CreateBundle();

  auto json_request = StartRequest(kStreamingJsonUrl, kResourceRequestId);
  auto js_request = StartRequest(kStreamingJsUrl, kResourceRequestId2);

  // Queue OnDataAvailableForOrb for `json_request` in `source_->pending_reads_`
  // while `js_request` is still awaiting `parser_->ParseResponse`.
  ASSERT_GT(bundle.size(), 4800u);
  WriteBundle(base::span(bundle).first<4800>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory_->PendingReadCountForTesting() == 2u; }));
  EXPECT_FALSE(json_request.client->has_received_response());
  EXPECT_FALSE(js_request.client->has_received_response());

  factory_ = nullptr;

  json_request.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            json_request.client->completion_status().error_code);
  EXPECT_FALSE(json_request.client->has_received_response());

  js_request.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            js_request.client->completion_status().error_code);
  EXPECT_FALSE(js_request.client->has_received_response());
}

TEST_F(WebBundleURLLoaderFactoryTest, BundleFetchErrorWhileOrbSniffingPending) {
  const GURL kStreamingJsonUrl("https://other.test/streaming-json");
  const GURL kStreamingJsUrl("https://other.test/streaming-js");

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kStreamingJsonUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-padding", std::string(4000, 'x')}},
                      "{\"data\": \"" + std::string(10000, 'a') + "\"}");
  builder.AddExchange(
      kStreamingJsUrl,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "const data = \"" + std::string(10000, 'a') + "\";");
  std::vector<uint8_t> bundle = builder.CreateBundle();

  auto json_request = StartRequest(kStreamingJsonUrl, kResourceRequestId);
  auto js_request = StartRequest(kStreamingJsUrl, kResourceRequestId2);

  ASSERT_GT(bundle.size(), 4800u);
  WriteBundle(base::span(bundle).first<4800>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory_->PendingReadCountForTesting() == 2u; }));
  EXPECT_FALSE(json_request.client->has_received_response());
  EXPECT_FALSE(js_request.client->has_received_response());

  // Report a bundle fetch error while the data pipe is still open and ORB
  // sniffing is pending in `source_->pending_reads_`.
  factory_->OnWebBundleFetchFailed();

  json_request.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            json_request.client->completion_status().error_code);
  EXPECT_FALSE(json_request.client->has_received_response());

  js_request.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            js_request.client->completion_status().error_code);
  EXPECT_FALSE(js_request.client->has_received_response());

  RunUntilBundleError();
  ASSERT_TRUE(last_bundle_error().has_value());
  EXPECT_EQ(mojom::WebBundleErrorType::kWebBundleFetchFailed,
            last_bundle_error()->first);
}

TEST_F(WebBundleURLLoaderFactoryTest, ClientDisconnectWhileOrbSniffingPending) {
  const GURL kStreamingJsonUrl("https://other.test/streaming-json");

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kStreamingJsonUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-padding", std::string(4000, 'x')}},
                      "{\"data\": \"" + std::string(10000, 'a') + "\"}");
  std::vector<uint8_t> bundle = builder.CreateBundle();

  auto request = StartRequest(kStreamingJsonUrl, kResourceRequestId);

  ASSERT_GT(bundle.size(), 4800u);
  WriteBundle(base::span(bundle).first<4800>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory_->PendingReadCountForTesting() == 1u; }));
  EXPECT_FALSE(request.client->has_received_response());

  // Client cancels request while sniffing is pending in
  // source_->pending_reads_. Wait for URLLoaderClient disconnect so URLLoader
  // is guaranteed to be destroyed (invalidating its WeakPtr) before the
  // remaining bundle bytes trigger OnDataAvailableForOrb.
  request.loader.reset();
  request.client->RunUntilDisconnect();
  EXPECT_TRUE(request.client->has_received_disconnect());
  EXPECT_FALSE(request.client->has_received_response());
  EXPECT_EQ(1u, factory_->PendingReadCountForTesting());
  request.client.reset();

  WriteBundle(base::span(bundle).subspan<4800>());
  FinishWritingBundle();
  RunUntilBundleLoadFinished();
  EXPECT_EQ(0u, factory_->PendingReadCountForTesting());
}

TEST_F(WebBundleURLLoaderFactoryTest, CrossOriginTruncatedSniffedPayload) {
  const GURL kCrossOriginDataUrl("https://other.test/data.bin");
  web_package::WebBundleBuilder builder;
  // Use a response header (~4000 bytes) + payload (500 bytes) whose combined
  // length exceeds INITIAL_BUFFER_SIZE_FOR_RESPONSE (4096 bytes), so that
  // parsing the response header succeeds while the payload itself is truncated.
  builder.AddExchange(kCrossOriginDataUrl,
                      {{":status", "200"},
                       {"content-type", "application/octet-stream"},
                       {"x-padding", std::string(4000, 'x')}},
                      "{\"data\": \"" + std::string(500, 'a') + "\"}");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  // Truncate 100 bytes from the end of the bundle (inside the 500-byte
  // payload).
  ASSERT_GT(bundle.size(), 100u);
  bundle.resize(bundle.size() - 100);
  WriteBundle(bundle);
  FinishWritingBundle();

  auto request = StartRequest(kCrossOriginDataUrl, kResourceRequestId);
  request.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_FALSE(request.client->has_received_response());
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

// Test fixture configured with CrossOriginEmbedderPolicy: require-corp.
class WebBundleURLLoaderFactoryRequireCorpTest
    : public WebBundleURLLoaderFactoryTest {
 public:
  WebBundleURLLoaderFactoryRequireCorpTest()
      : WebBundleURLLoaderFactoryTest(RequireCorpPolicy()) {}

 private:
  static CrossOriginEmbedderPolicy RequireCorpPolicy() {
    CrossOriginEmbedderPolicy coep;
    coep.value = mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
    return coep;
  }
};

// Regression test for crbug.com/349994197.
//
// Previously, WebBundleURLLoaderFactory held a raw pointer to
// mojom::CrossOriginEmbedderPolicyReporter borrowed from CorsURLLoaderFactory.
// If CorsURLLoaderFactory was destroyed before WebBundleURLLoaderFactory,
// attempting to report a CORP violation via the dangling pointer resulted in a
// Use-After-Free (UAF).
//
// This test verifies that WebBundleURLLoaderFactory safely retains its own
// cloned reporter remote even after the original reporter remote (owned by
// CorsURLLoaderFactory) is destroyed:
// 1. Simulate CorsURLLoaderFactory destruction via ResetOriginalCoepReporter().
// 2. Issue a cross-origin subresource request that violates CORP.
// 3. Verify that the request is blocked and the CORP violation report is
//    safely delivered through the cloned remote without crashing.
TEST_F(WebBundleURLLoaderFactoryRequireCorpTest, ClonedReporterDestruction) {
  // Step 1: Simulate CorsURLLoaderFactory being destroyed while
  // WebBundleURLLoaderFactory is still alive.
  ResetOriginalCoepReporter();

  WriteBundle(CreateCrossOriginBundle());
  FinishWritingBundle();

  // Step 2: Issue a cross-origin subresource request that lacks a CORP header.
  // Under COEP: require-corp, this request will be blocked by CORP, which
  // invokes coep_reporter_->QueueCorpViolationReport().
  network::ResourceRequest request =
      CreateRequest(GURL(kCrossOriginJsUrl), kResourceRequestId);
  request.mode = mojom::RequestMode::kNoCors;
  request.destination = mojom::RequestDestination::kScript;

  auto result = StartRequest(request);
  result.client->RunUntilComplete();
  RunUntilCoepReport();

  // Step 3: Verify the request is blocked and the report is safely delivered.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            result.client->completion_status().error_code);
  EXPECT_THAT(coep_reports(), ::testing::ElementsAre(GURL(kCrossOriginJsUrl)));
}

TEST_F(WebBundleURLLoaderFactoryTest, UnencodedDigestParsed) {
  constexpr char kValidDigestHeader[] =
      "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"},
                       {"content-type", "application/json"},
                       {"unencoded-digest", kValidDigestHeader}},
                      R"({"hello": "world"})");
  builder.AddExchange(kResourceUrl2,
                      {{":status", "200"},
                       {"content-type", "application/json"},
                       {"unencoded-digest", "malformed-digest"}},
                      R"({"hello": "world"})");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  auto valid_request = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  valid_request.client->RunUntilComplete();
  EXPECT_EQ(net::OK, valid_request.client->completion_status().error_code);
  ASSERT_TRUE(valid_request.client->response_head()->unencoded_digests);
  EXPECT_EQ(
      1u,
      valid_request.client->response_head()->unencoded_digests->digests.size());
  EXPECT_TRUE(
      valid_request.client->response_head()->unencoded_digests->issues.empty());

  auto malformed_request =
      StartRequest(GURL(kResourceUrl2), kResourceRequestId2);
  malformed_request.client->RunUntilComplete();
  EXPECT_EQ(net::OK, malformed_request.client->completion_status().error_code);
  ASSERT_TRUE(malformed_request.client->response_head()->unencoded_digests);
  EXPECT_TRUE(malformed_request.client->response_head()
                  ->unencoded_digests->digests.empty());
  EXPECT_FALSE(malformed_request.client->response_head()
                   ->unencoded_digests->issues.empty());
}

TEST_F(WebBundleURLLoaderFactoryTest, SRIMessageSignatureValid) {
  constexpr char kPublicKey[] = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
  constexpr char kValidDigestHeader[] =
      "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
  constexpr char kValidSignatureInputHeader[] =
      "signature=(\"unencoded-digest\";sf);"
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";"
      "tag=\"ed25519-integrity\"";
  constexpr char kValidSignatureHeader[] =
      "signature=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/"
      "JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:";

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"},
                       {"content-type", "application/json"},
                       {"unencoded-digest", kValidDigestHeader},
                       {"signature-input", kValidSignatureInputHeader},
                       {"signature", kValidSignatureHeader}},
                      R"({"hello": "world"})");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  network::ResourceRequest request =
      CreateRequest(GURL(kResourceUrl), kResourceRequestId);
  std::optional<std::vector<uint8_t>> decoded_key =
      base::Base64Decode(kPublicKey);
  ASSERT_TRUE(decoded_key.has_value());
  request.expected_public_keys = {*decoded_key};

  auto result = StartRequest(request);
  result.client->RunUntilComplete();

  EXPECT_EQ(net::OK, result.client->completion_status().error_code);
  ASSERT_TRUE(result.client->response_head()->unencoded_digests);
  EXPECT_EQ(1u,
            result.client->response_head()->unencoded_digests->digests.size());
  std::string body;
  EXPECT_TRUE(mojo::BlockingCopyToString(result.client->response_body_release(),
                                         &body));
  EXPECT_EQ(R"({"hello": "world"})", body);
}

TEST_F(WebBundleURLLoaderFactoryTest, SRIMessageSignatureInvalidBlocked) {
  constexpr char kValidDigestHeader[] =
      "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
  constexpr char kValidSignatureInputHeader[] =
      "signature=(\"unencoded-digest\";sf);"
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";"
      "tag=\"ed25519-integrity\"";
  constexpr char kInvalidSignatureHeader[] =
      "signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj"
      "5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==:";

  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"},
                       {"content-type", "application/json"},
                       {"unencoded-digest", kValidDigestHeader},
                       {"signature-input", kValidSignatureInputHeader},
                       {"signature", kInvalidSignatureHeader}},
                      R"({"hello": "world"})");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  auto result = StartRequest(GURL(kResourceUrl), kResourceRequestId);
  result.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            result.client->completion_status().error_code);
  EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
            result.client->completion_status().blocked_by_response_reason);
}

TEST_F(WebBundleURLLoaderFactoryTest,
       SRIMessageSignatureExpectedPublicKeyMismatchOrMissingBlocked) {
  constexpr char kOtherPublicKey[] =
      "xDnP380zcL4rJ76rXYjeHlfMyPZEOqpJYjsjEppbuXE=";
  constexpr char kValidDigestHeader[] =
      "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
  constexpr char kValidSignatureInputHeader[] =
      "signature=(\"unencoded-digest\";sf);"
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";"
      "tag=\"ed25519-integrity\"";
  constexpr char kValidSignatureHeader[] =
      "signature=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/"
      "JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:";

  web_package::WebBundleBuilder builder;
  // Signed with kPublicKey, not kOtherPublicKey.
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"},
                       {"content-type", "application/json"},
                       {"unencoded-digest", kValidDigestHeader},
                       {"signature-input", kValidSignatureInputHeader},
                       {"signature", kValidSignatureHeader}},
                      R"({"hello": "world"})");
  // Unsigned response.
  builder.AddExchange(
      kResourceUrl2, {{":status", "200"}, {"content-type", "application/json"}},
      R"({"hello": "world"})");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  std::optional<std::vector<uint8_t>> other_key =
      base::Base64Decode(kOtherPublicKey);
  ASSERT_TRUE(other_key.has_value());

  // 1. Signed with a different public key than expected -> blocked.
  network::ResourceRequest wrong_key_request =
      CreateRequest(GURL(kResourceUrl), kResourceRequestId);
  wrong_key_request.expected_public_keys = {*other_key};
  auto wrong_key_result = StartRequest(wrong_key_request);
  wrong_key_result.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            wrong_key_result.client->completion_status().error_code);
  EXPECT_EQ(
      mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
      wrong_key_result.client->completion_status().blocked_by_response_reason);

  // 2. Unsigned response when client requires public key integrity -> blocked.
  network::ResourceRequest unsigned_request =
      CreateRequest(GURL(kResourceUrl2), kResourceRequestId2);
  unsigned_request.expected_public_keys = {*other_key};
  auto unsigned_result = StartRequest(unsigned_request);
  unsigned_result.client->RunUntilComplete();
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            unsigned_result.client->completion_status().error_code);
  EXPECT_EQ(
      mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
      unsigned_result.client->completion_status().blocked_by_response_reason);
}

}  // namespace network
