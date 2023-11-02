// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_url_loader_factory.h"

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/mock_web_bundle_reader_factory.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

class WebBundleURLLoaderFactoryTest : public testing::Test {
 public:
  WebBundleURLLoaderFactoryTest() {}

  WebBundleURLLoaderFactoryTest(const WebBundleURLLoaderFactoryTest&) = delete;
  WebBundleURLLoaderFactoryTest& operator=(
      const WebBundleURLLoaderFactoryTest&) = delete;

  void SetUp() override {
    mock_factory_ = MockWebBundleReaderFactory::Create();
    auto reader = mock_factory_->CreateReader(body_);
    reader_ = reader.get();
    loader_factory_ = std::make_unique<WebBundleURLLoaderFactory>(
        std::move(reader), FrameTreeNode::kFrameTreeNodeInvalidId);

    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert({primary_url_,
                  web_package::mojom::BundleResponseLocation::New(573u, 765u)});

    web_package::mojom::BundleMetadataPtr metadata =
        web_package::mojom::BundleMetadata::New();
    metadata->primary_url = primary_url_;
    metadata->requests = std::move(items);

    base::RunLoop run_loop;
    mock_factory_->ReadAndFullfillMetadata(
        reader_, std::move(metadata),
        base::BindOnce(
            [](base::OnceClosure quit_closure,
               web_package::mojom::BundleMetadataParseErrorPtr error) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();

    // Set some useful default values for |resource_request_|.
    resource_request_.url = primary_url_;
    resource_request_.method = net::HttpRequestHeaders::kGetMethod;
  }

  void TearDown() override {
    // Shut down the loader factory and allow its cleanup tasks in the
    // ThreadPool to run so that temp dirs can be deleted.
    loader_factory_.reset();
    task_environment_.RunUntilIdle();
  }

  // This function creates a URLLoader with |resource_request_|, and simulates
  // a response for WebBundleReader::ReadResponse with |response| if it
  // is given. |response| can contain nullptr to simulate the case ReadResponse
  // fails.
  mojo::Remote<network::mojom::URLLoader> CreateLoaderAndStart(
      absl::optional<web_package::mojom::BundleResponsePtr> response,
      bool clone = false) {
    mojo::Remote<network::mojom::URLLoader> loader;

    if (clone) {
      mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
      loader_factory_->Clone(loader_factory.BindNewPipeAndPassReceiver());
      loader_factory->CreateLoaderAndStart(
          loader.BindNewPipeAndPassReceiver(),
          /*request_id=*/0, /*options=*/0, resource_request_,
          test_client_.CreateRemote(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));
    } else {
      loader_factory_->CreateLoaderAndStart(
          loader.BindNewPipeAndPassReceiver(),
          /*request_id=*/0, /*options=*/0, resource_request_,
          test_client_.CreateRemote(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));
    }

    if (response)
      mock_factory_->FullfillResponse(
          web_package::mojom::BundleResponseLocation::New(573u, 765u),
          std::move(*response));
    return loader;
  }

  const GURL& GetPrimaryURL() const { return primary_url_; }
  const std::string& GetBody() const { return body_; }

  void RunAndCheck(int expected_response_code,
                   const std::string expected_body) {
    test_client_.RunUntilResponseBodyArrived();

    EXPECT_TRUE(test_client_.has_received_response());
    EXPECT_FALSE(test_client_.has_received_redirect());
    EXPECT_FALSE(test_client_.has_received_upload_progress());

    ASSERT_TRUE(test_client_.response_head()->headers);
    ASSERT_TRUE(test_client_.response_body());
    ASSERT_FALSE(test_client_.cached_metadata().has_value());

    EXPECT_EQ(expected_response_code,
              test_client_.response_head()->headers->response_code());

    if (!expected_body.empty()) {
      std::vector<char> buffer(expected_body.size() * 2);
      uint32_t num_bytes = buffer.size();
      MojoResult result;
      do {
        base::RunLoop().RunUntilIdle();
        result = test_client_.response_body().ReadData(
            buffer.data(), &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      } while (result == MOJO_RESULT_SHOULD_WAIT);

      EXPECT_EQ(MOJO_RESULT_OK, result);
      EXPECT_EQ(expected_body.size(), num_bytes);
      EXPECT_EQ(expected_body, std::string(buffer.data(), num_bytes));
    }

    test_client_.RunUntilComplete();

    ASSERT_TRUE(test_client_.has_received_completion());
    EXPECT_EQ(net::OK, test_client_.completion_status().error_code);
  }

  void RunAndCheckFailure(int net_error) {
    test_client_.RunUntilComplete();
    ASSERT_TRUE(test_client_.has_received_completion());
    EXPECT_EQ(net_error, test_client_.completion_status().error_code);
  }

  void SetFallbackFactory(
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
    loader_factory_->SetFallbackFactory(std::move(fallback_factory));
  }

  const network::TestURLLoaderClient& GetTestClient() const {
    return test_client_;
  }

 protected:
  network::ResourceRequest resource_request_;

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockWebBundleReaderFactory> mock_factory_;
  std::unique_ptr<WebBundleURLLoaderFactory> loader_factory_;
  raw_ptr<WebBundleReader> reader_;
  network::TestURLLoaderClient test_client_;
  const std::string body_ = std::string("present day, present time");
  const GURL primary_url_ = GURL("https://test.example.org/");
};

TEST_F(WebBundleURLLoaderFactoryTest, CreateEntryLoader) {
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(WebBundleURLLoaderFactoryTest, RangeRequest) {
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  resource_request_.headers.SetHeader(net::HttpRequestHeaders::kRange,
                                      "bytes=10-19");

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(206, GetBody().substr(10, 10));
  EXPECT_EQ(10, GetTestClient().response_head()->headers->GetContentLength());
  std::string content_range;
  EXPECT_TRUE(GetTestClient().response_head()->headers->EnumerateHeader(
      nullptr, net::HttpResponseHeaders::kContentRange, &content_range));
  EXPECT_EQ("bytes 10-19/25", content_range);
}

TEST_F(WebBundleURLLoaderFactoryTest,
       CreateEntryLoaderForURLContainingUserAndPass) {
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  resource_request_.url = GURL("https://user:pass@test.example.org/");

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(WebBundleURLLoaderFactoryTest,
       CreateEntryLoaderForURLContainingFragment) {
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  resource_request_.url = GURL("https://test.example.org/#test");

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(WebBundleURLLoaderFactoryTest, CreateEntryLoaderAndFailToReadResponse) {
  auto loader = CreateLoaderAndStart(/*response=*/nullptr);

  RunAndCheckFailure(net::ERR_INVALID_WEB_BUNDLE);
}

TEST_F(WebBundleURLLoaderFactoryTest, CreateLoaderForPost) {
  // URL should match, but POST method should not be handled by the EntryLoader.
  resource_request_.method = "POST";
  auto loader = CreateLoaderAndStart(/*response=*/absl::nullopt);

  RunAndCheckFailure(net::ERR_FAILED);
}

TEST_F(WebBundleURLLoaderFactoryTest, CreateLoaderForNotSupportedURL) {
  resource_request_.url = GURL("https://test.example.org/nowhere");
  auto loader = CreateLoaderAndStart(/*response=*/absl::nullopt);

  RunAndCheckFailure(net::ERR_FAILED);
}

TEST_F(WebBundleURLLoaderFactoryTest, CreateFallbackLoader) {
  // Create a TestURLLoaderFactory, and set it to the
  // WebBundleURLLoaderFactory as a fallback factory.
  auto test_factory = std::make_unique<network::TestURLLoaderFactory>();
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  test_factory->Clone(factory.BindNewPipeAndPassReceiver());

  SetFallbackFactory(std::move(factory));

  // Access to the 404 address for the WebBundle, so to be handled by
  // the fallback factory set above.
  const std::string url_string = "https://test.example.org/somewhere";
  resource_request_.url = GURL(url_string);
  auto loader = CreateLoaderAndStart(absl::nullopt);
  ASSERT_EQ(1, test_factory->NumPending());

  // Reply with a mock response.
  test_factory->SimulateResponseForPendingRequest(url_string, GetBody(),
                                                  net::HTTP_OK);

  RunAndCheck(200, GetBody());
}

TEST_F(WebBundleURLLoaderFactoryTest, CreateByClonedFactory) {
  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  auto loader = CreateLoaderAndStart(std::move(response), /*clone=*/true);

  RunAndCheck(200, GetBody());
}

}  // namespace

}  // namespace content
