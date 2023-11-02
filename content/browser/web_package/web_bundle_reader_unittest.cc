// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_reader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/browser/web_package/mock_web_bundle_reader_factory.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "mojo/public/c/system/data_pipe.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

class WebBundleReaderTest : public testing::Test {
 protected:
  void SetUp() override {
    reader_factory_ = MockWebBundleReaderFactory::Create();
    reader_ = reader_factory_->CreateReader(body_);
  }

  void TearDown() override {
    reader_.reset();
    // Allow cleanup tasks posted by the reader's dtor to run.
    task_environment_.RunUntilIdle();
  }

  void ReadMetadata() {
    base::flat_map<GURL, web_package::mojom::BundleResponseLocationPtr> items;
    items.insert({primary_url_,
                  web_package::mojom::BundleResponseLocation::New(573u, 765u)});

    web_package::mojom::BundleMetadataPtr metadata =
        web_package::mojom::BundleMetadata::New();
    metadata->primary_url = primary_url_;
    metadata->requests = std::move(items);
    reader_factory_->ReadAndFullfillMetadata(
        reader_.get(), std::move(metadata),
        base::BindOnce(
            [](web_package::mojom::BundleMetadataParseErrorPtr error) {
              EXPECT_FALSE(error);
            }));
  }
  WebBundleReader* GetReader() { return reader_.get(); }
  MockWebBundleReaderFactory* GetMockFactory() { return reader_factory_.get(); }
  const GURL& GetPrimaryURL() const { return primary_url_; }
  const std::string& GetBody() const { return body_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockWebBundleReaderFactory> reader_factory_;
  scoped_refptr<WebBundleReader> reader_;
  const GURL primary_url_ = GURL("https://test.example.org/");
  const std::string body_ = std::string("hello new open world.");
};

TEST_F(WebBundleReaderTest, ReadMetadata) {
  ReadMetadata();
  EXPECT_EQ(GetPrimaryURL(), GetReader()->GetPrimaryURL());
  EXPECT_TRUE(GetReader()->HasEntry(GetPrimaryURL()));
  EXPECT_TRUE(
      GetReader()->HasEntry(GURL("https://user:pass@test.example.org/")));
  EXPECT_TRUE(GetReader()->HasEntry(GURL("https://test.example.org/#ref")));
  EXPECT_FALSE(GetReader()->HasEntry(GURL("https://test.example.org/404")));
  EXPECT_EQ(GetReader()->GetEntries(),
            std::vector({GURL("https://test.example.org")}));
}

TEST_F(WebBundleReaderTest, ReadResponse) {
  ReadMetadata();
  ASSERT_TRUE(GetReader()->HasEntry(GetPrimaryURL()));

  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0xdead;
  response->payload_length = 0xbeaf;

  network::ResourceRequest resource_request;
  resource_request.url = GetPrimaryURL();

  GetMockFactory()->ReadAndFullfillResponse(
      GetReader(), resource_request,
      web_package::mojom::BundleResponseLocation::New(573u, 765u),
      std::move(response),
      base::BindOnce([](web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error) {
        EXPECT_TRUE(response);
        EXPECT_FALSE(error);
        if (response) {
          EXPECT_EQ(200, response->response_code);
          EXPECT_EQ(0xdeadu, response->payload_offset);
          EXPECT_EQ(0xbeafu, response->payload_length);
        }
      }));
}

TEST_F(WebBundleReaderTest, ReadResponseForURLContainingUserAndPass) {
  GURL url = GURL("https://user:pass@test.example.org/");

  ReadMetadata();
  ASSERT_TRUE(GetReader()->HasEntry(url));

  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0xdead;
  response->payload_length = 0xbeaf;

  network::ResourceRequest resource_request;
  resource_request.url = url;

  GetMockFactory()->ReadAndFullfillResponse(
      GetReader(), resource_request,
      web_package::mojom::BundleResponseLocation::New(573u, 765u),
      std::move(response),
      base::BindOnce([](web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error) {
        EXPECT_TRUE(response);
        EXPECT_FALSE(error);
        if (response) {
          EXPECT_EQ(200, response->response_code);
          EXPECT_EQ(0xdeadu, response->payload_offset);
          EXPECT_EQ(0xbeafu, response->payload_length);
        }
      }));
}

TEST_F(WebBundleReaderTest, ReadResponseForURLContainingFragment) {
  GURL url = GURL("https://test.example.org/#fragment");

  ReadMetadata();
  ASSERT_TRUE(GetReader()->HasEntry(url));

  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0xdead;
  response->payload_length = 0xbeaf;

  network::ResourceRequest resource_request;
  resource_request.url = url;

  GetMockFactory()->ReadAndFullfillResponse(
      GetReader(), resource_request,
      web_package::mojom::BundleResponseLocation::New(573u, 765u),
      std::move(response),
      base::BindOnce([](web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error) {
        EXPECT_TRUE(response);
        EXPECT_FALSE(error);
        if (response) {
          EXPECT_EQ(200, response->response_code);
          EXPECT_EQ(0xdeadu, response->payload_offset);
          EXPECT_EQ(0xbeafu, response->payload_length);
        }
      }));
}

TEST_F(WebBundleReaderTest, ReadResponseBody) {
  ReadMetadata();

  web_package::mojom::BundleResponsePtr response =
      web_package::mojom::BundleResponse::New();
  constexpr size_t expected_offset = 4;
  const size_t expected_length = GetBody().size() - 8;
  response->payload_offset = expected_offset;
  response->payload_length = expected_length;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.element_num_bytes = 1;
  options.capacity_num_bytes = response->payload_length + 1;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, producer, consumer));

  base::RunLoop run_loop;
  net::Error callback_result;
  GetReader()->ReadResponseBody(
      std::move(response), std::move(producer),
      base::BindOnce(
          [](base::OnceClosure quit_closure, net::Error* callback_result,
             net::Error result) {
            *callback_result = result;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &callback_result));
  run_loop.Run();
  ASSERT_EQ(net::OK, callback_result);

  std::vector<char> buffer(expected_length);
  uint32_t bytes_read = buffer.size();
  MojoResult read_result =
      consumer->ReadData(buffer.data(), &bytes_read, /*flags=*/0);
  ASSERT_EQ(MOJO_RESULT_OK, read_result);
  ASSERT_EQ(buffer.size(), bytes_read);
  EXPECT_EQ(GetBody().substr(expected_offset, expected_length),
            std::string(buffer.data(), bytes_read));
}

}  // namespace

}  // namespace content
