// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_url_loader_factory.h"

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/mock_bundled_exchanges_reader_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class BundledExchangesURLLoaderFactoryTest : public testing::Test {
 public:
  BundledExchangesURLLoaderFactoryTest() {}

  void SetUp() override {
    mock_factory_ = MockBundledExchangesReaderFactory::Create();
    auto reader = mock_factory_->CreateReader(body_);
    reader_ = reader.get();
    loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
        std::move(reader), FrameTreeNode::kFrameTreeNodeInvalidId);

    base::flat_map<GURL, data_decoder::mojom::BundleIndexValuePtr> items;
    data_decoder::mojom::BundleIndexValuePtr item =
        data_decoder::mojom::BundleIndexValue::New();
    item->response_locations.push_back(
        data_decoder::mojom::BundleResponseLocation::New(0u, 0u));
    items.insert({primary_url_, std::move(item)});

    data_decoder::mojom::BundleMetadataPtr metadata =
        data_decoder::mojom::BundleMetadata::New();
    metadata->primary_url = primary_url_;
    metadata->requests = std::move(items);

    base::RunLoop run_loop;
    mock_factory_->ReadAndFullfillMetadata(
        reader_, std::move(metadata),
        base::BindOnce(
            [](base::Closure quit_closure,
               data_decoder::mojom::BundleMetadataParseErrorPtr error) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();

    // Set some useful default values for |resource_request_|.
    resource_request_.url = primary_url_;
    resource_request_.method = net::HttpRequestHeaders::kGetMethod;
  }

  // This function creates a URLLoader with |resource_request_|, and simulates
  // a response for BundledExchangesReader::ReadResponse with |response| if it
  // is given. |response| can contain nullptr to simulate the case ReadResponse
  // fails.
  mojo::Remote<network::mojom::URLLoader> CreateLoaderAndStart(
      base::Optional<data_decoder::mojom::BundleResponsePtr> response,
      bool clone = false) {
    mojo::Remote<network::mojom::URLLoader> loader;

    if (clone) {
      mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
      loader_factory_->Clone(loader_factory.BindNewPipeAndPassReceiver());
      loader_factory->CreateLoaderAndStart(
          loader.BindNewPipeAndPassReceiver(),
          /*routing_id=*/0, /*request_id=*/0, /*options=*/0, resource_request_,
          test_client_.CreateRemote(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));

    } else {
      loader_factory_->CreateLoaderAndStart(
          loader.BindNewPipeAndPassReceiver(),
          /*routing_id=*/0, /*request_id=*/0, /*options=*/0, resource_request_,
          test_client_.CreateRemote(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));
    }

    if (response)
      mock_factory_->FullfillResponse(std::move(*response), base::DoNothing());
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
    EXPECT_FALSE(test_client_.has_received_cached_metadata());

    ASSERT_TRUE(test_client_.response_head()->headers);
    ASSERT_TRUE(test_client_.response_body());

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
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockBundledExchangesReaderFactory> mock_factory_;
  std::unique_ptr<BundledExchangesURLLoaderFactory> loader_factory_;
  BundledExchangesReader* reader_;
  network::TestURLLoaderClient test_client_;
  const std::string body_ = std::string("present day, present time");
  const GURL primary_url_ = GURL("https://test.example.org/");

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesURLLoaderFactoryTest);
};

TEST_F(BundledExchangesURLLoaderFactoryTest, CreateEntryLoader) {
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(BundledExchangesURLLoaderFactoryTest, RangeRequest) {
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
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

TEST_F(BundledExchangesURLLoaderFactoryTest,
       CreateEntryLoaderForURLContainingUserAndPass) {
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  resource_request_.url = GURL("https://user:pass@test.example.org/");

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(BundledExchangesURLLoaderFactoryTest,
       CreateEntryLoaderForURLContainingFragment) {
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  resource_request_.url = GURL("https://test.example.org/#test");

  auto loader = CreateLoaderAndStart(std::move(response));

  RunAndCheck(200, GetBody());
}

TEST_F(BundledExchangesURLLoaderFactoryTest,
       CreateEntryLoaderAndFailToReadResponse) {
  auto loader = CreateLoaderAndStart(/*response=*/nullptr);

  RunAndCheckFailure(net::ERR_INVALID_BUNDLED_EXCHANGES);
}

TEST_F(BundledExchangesURLLoaderFactoryTest, CreateLoaderForPost) {
  // URL should match, but POST method should not be handled by the EntryLoader.
  resource_request_.method = "POST";
  auto loader = CreateLoaderAndStart(/*response=*/base::nullopt);

  RunAndCheckFailure(net::ERR_FAILED);
}

TEST_F(BundledExchangesURLLoaderFactoryTest, CreateLoaderForNotSupportedURL) {
  resource_request_.url = GURL("https://test.example.org/nowhere");
  auto loader = CreateLoaderAndStart(/*response=*/base::nullopt);

  RunAndCheckFailure(net::ERR_FAILED);
}

TEST_F(BundledExchangesURLLoaderFactoryTest, CreateFallbackLoader) {
  // Create a TestURLLoaderFactory, and set it to the
  // BundledExchangesURLLoaderFactory as a fallback factory.
  auto test_factory = std::make_unique<network::TestURLLoaderFactory>();
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  test_factory->Clone(factory.BindNewPipeAndPassReceiver());

  SetFallbackFactory(std::move(factory));

  // Access to the 404 address for the BundledExchanges, so to be handled by
  // the fallback factory set above.
  const std::string url_string = "https://test.example.org/somewhere";
  resource_request_.url = GURL(url_string);
  auto loader = CreateLoaderAndStart(base::nullopt);
  ASSERT_EQ(1, test_factory->NumPending());

  // Reply with a mock response.
  test_factory->SimulateResponseForPendingRequest(url_string, GetBody(),
                                                  net::HTTP_OK);

  RunAndCheck(200, GetBody());
}

TEST_F(BundledExchangesURLLoaderFactoryTest, CreateByClonedFactory) {
  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0;
  response->payload_length = GetBody().size();

  auto loader = CreateLoaderAndStart(std::move(response), /*clone=*/true);

  RunAndCheck(200, GetBody());
}

}  // namespace

}  // namespace content
