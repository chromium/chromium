// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/web_bundle_subresource_loader.h"

#include "base/test/task_environment.h"
#include "components/web_package/test_support/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

const char kResourceUrl[] = "https://example.com/";
const char kResourceUrl2[] = "https://example.com/another";
const char kResourceUrl3[] = "https://example.com/yetanother";

std::vector<uint8_t> CreateSmallBundle() {
  web_package::test::WebBundleBuilder builder(kResourceUrl,
                                              "" /* manifest_url */);
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  return builder.CreateBundle();
}

std::vector<uint8_t> CreateLargeBundle() {
  web_package::test::WebBundleBuilder builder(kResourceUrl,
                                              "" /* manifest_url */);
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

}  // namespace

class WebBundleSubresourceLoaderFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(CreateDataPipe(nullptr, &bundle_data_destination_, &consumer),
              MOJO_RESULT_OK);
    CreateWebBundleSubresourceLoaderFactory(
        loader_factory_.BindNewPipeAndPassReceiver(), std::move(consumer),
        base::BindRepeating(
            &WebBundleSubresourceLoaderFactoryTest::OnWebBundleError,
            base::Unretained(this)));
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

  StartRequestResult StartRequest(const GURL& url) {
    return StartRequestWithLoaderFactory(loader_factory_, url);
  }

  void RunUntilBundleError() {
    if (last_bundle_error_.has_value())
      return;
    base::RunLoop run_loop;
    quit_closure_for_bundle_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const base::Optional<std::pair<WebBundleErrorType, WTF::String>>&
  last_bundle_error() const {
    return last_bundle_error_;
  }

 protected:
  StartRequestResult StartRequestWithLoaderFactory(
      mojo::Remote<network::mojom::URLLoaderFactory>& factory,
      const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    StartRequestResult result;
    result.client = std::make_unique<network::TestURLLoaderClient>();
    factory->CreateLoaderAndStart(
        result.loader.BindNewPipeAndPassReceiver(), 0 /* routing_id */,
        0 /* request_id */, 0 /* options */, request,
        result.client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return result;
  }

  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory_;

 private:
  void OnWebBundleError(WebBundleErrorType type, const WTF::String& message) {
    last_bundle_error_ = std::make_pair(type, message);
    if (quit_closure_for_bundle_error_)
      std::move(quit_closure_for_bundle_error_).Run();
  }

  mojo::ScopedDataPipeProducerHandle bundle_data_destination_;
  base::test::TaskEnvironment task_environment;
  base::Optional<std::pair<WebBundleErrorType, WTF::String>> last_bundle_error_;
  base::OnceClosure quit_closure_for_bundle_error_;
};

TEST_F(WebBundleSubresourceLoaderFactoryTest, Basic) {
  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL(kResourceUrl));
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
  EXPECT_FALSE(last_bundle_error().has_value());
  std::string body;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      request.client->response_body_release(), &body));
  EXPECT_EQ("body", body);
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, Clone) {
  mojo::Remote<network::mojom::URLLoaderFactory> cloned_factory_;
  loader_factory_->Clone(cloned_factory_.BindNewPipeAndPassReceiver());

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  auto request =
      StartRequestWithLoaderFactory(cloned_factory_, GURL(kResourceUrl));
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
  EXPECT_FALSE(last_bundle_error().has_value());
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, MetadataParseError) {
  auto request = StartRequest(GURL(kResourceUrl));

  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle[4] ^= 1;  // Mutate magic bytes.
  WriteBundle(bundle);
  FinishWritingBundle();

  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            WebBundleErrorType::kMetadataParseError);
  EXPECT_EQ(last_bundle_error()->second, "Wrong magic bytes.");

  // Requests made after metadata parse error should also fail.
  auto request2 = StartRequest(GURL(kResourceUrl));
  request2.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request2.client->completion_status().error_code);
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, ResponseParseError) {
  web_package::test::WebBundleBuilder builder(kResourceUrl,
                                              "" /* manifest_url */);
  // An invalid response.
  builder.AddExchange(kResourceUrl, {{":status", "0"}}, "body");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL(kResourceUrl));
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second,
            ":status must be 3 ASCII decimal digits.");
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, ResourceNotFoundInBundle) {
  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL("https://example.com/no-such-resource"));
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first, WebBundleErrorType::kResourceNotFound);
  EXPECT_EQ(
      last_bundle_error()->second,
      "https://example.com/no-such-resource is not found in the WebBundle.");
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, RedirectResponseIsNotAllowed) {
  web_package::test::WebBundleBuilder builder(kResourceUrl,
                                              "" /* manifest_url */);
  builder.AddExchange(kResourceUrl,
                      {{":status", "301"}, {"location", kResourceUrl2}}, "");
  builder.AddExchange(kResourceUrl2,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  WriteBundle(builder.CreateBundle());
  FinishWritingBundle();

  auto request = StartRequest(GURL(kResourceUrl));
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second, "Invalid response code 301");
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, StartRequestBeforeReadingBundle) {
  auto request = StartRequest(GURL(kResourceUrl));

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();
  request.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request.client->completion_status().error_code);
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, MultipleRequests) {
  auto request1 = StartRequest(GURL(kResourceUrl));
  auto request2 = StartRequest(GURL(kResourceUrl2));

  std::vector<uint8_t> bundle = CreateLargeBundle();
  // Write the first 10kB of the bundle in which the bundle's metadata and the
  // response for kResourceUrl are included.
  ASSERT_GT(bundle.size(), 10000U);
  WriteBundle(base::make_span(bundle).subspan(0, 10000));
  request1.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request1.client->completion_status().error_code);
  EXPECT_FALSE(request2.client->has_received_completion());

  // Write the rest of the data.
  WriteBundle(base::make_span(bundle).subspan(10000));
  FinishWritingBundle();
  request2.client->RunUntilComplete();

  EXPECT_EQ(net::OK, request2.client->completion_status().error_code);
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, CancelRequest) {
  auto request_to_complete1 = StartRequest(GURL(kResourceUrl));
  auto request_to_complete2 = StartRequest(GURL(kResourceUrl2));
  auto request_to_cancel1 = StartRequest(GURL(kResourceUrl));
  auto request_to_cancel2 = StartRequest(GURL(kResourceUrl2));
  auto request_to_cancel3 = StartRequest(GURL(kResourceUrl3));

  // Cancel request before getting metadata.
  request_to_cancel1.loader.reset();

  std::vector<uint8_t> bundle = CreateLargeBundle();
  // Write the first 10kB of the bundle in which the bundle's metadata, response
  // for kResourceUrl, and response header for kResourceUrl2 are included.
  ASSERT_GT(bundle.size(), 10000U);
  WriteBundle(base::make_span(bundle).subspan(0, 10000));

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

TEST_F(WebBundleSubresourceLoaderFactoryTest,
       FactoryDestructionCancelsInflightRequests) {
  auto request = StartRequest(GURL(kResourceUrl));

  loader_factory_.reset();

  WriteBundle(CreateSmallBundle());
  FinishWritingBundle();
  request.client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, request.client->completion_status().error_code);
}

TEST_F(WebBundleSubresourceLoaderFactoryTest, TruncatedBundle) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  // Truncate in the middle of responses section.
  bundle.resize(bundle.size() - 10);
  WriteBundle(std::move(bundle));
  FinishWritingBundle();

  auto request = StartRequest(GURL(kResourceUrl));
  request.client->RunUntilComplete();
  RunUntilBundleError();

  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            request.client->completion_status().error_code);
  EXPECT_EQ(last_bundle_error()->first,
            WebBundleErrorType::kResponseParseError);
  EXPECT_EQ(last_bundle_error()->second, "Error reading response header.");
}

}  // namespace blink
