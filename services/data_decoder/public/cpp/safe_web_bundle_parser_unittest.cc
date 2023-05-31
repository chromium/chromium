// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace data_decoder {

namespace {

using testing::UnorderedElementsAreArray;

constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";

base::File OpenTestFile(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));
  test_data_dir = test_data_dir.Append(path);
  return base::File(test_data_dir,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

class MockFactory final : public web_package::mojom::WebBundleParserFactory {
 public:
  class MockParser final : public web_package::mojom::WebBundleParser {
   public:
    explicit MockParser(
        mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver)
        : receiver_(this, std::move(receiver)) {}

    MockParser(const MockParser&) = delete;
    MockParser& operator=(const MockParser&) = delete;

    bool IsParseIntegrityBlockCalled() {
      return !integrity_block_callback_.is_null();
    }
    bool IsParseMetadataCalled() { return !metadata_callback_.is_null(); }
    bool IsParseResponseCalled() { return !response_callback_.is_null(); }

    void Disconnect() { receiver_.reset(); }

   private:
    // web_package::mojom::WebBundleParser implementation.
    void ParseIntegrityBlock(ParseIntegrityBlockCallback callback) override {
      integrity_block_callback_ = std::move(callback);
    }
    void ParseMetadata(absl::optional<uint64_t> offset,
                       ParseMetadataCallback callback) override {
      metadata_callback_ = std::move(callback);
    }
    void ParseResponse(uint64_t response_offset,
                       uint64_t response_length,
                       ParseResponseCallback callback) override {
      response_callback_ = std::move(callback);
    }

    ParseIntegrityBlockCallback integrity_block_callback_;
    ParseMetadataCallback metadata_callback_;
    ParseResponseCallback response_callback_;
    mojo::Receiver<web_package::mojom::WebBundleParser> receiver_;
  };

  MockFactory() {}

  MockFactory(const MockFactory&) = delete;
  MockFactory& operator=(const MockFactory&) = delete;

  void AddReceiver(
      mojo::PendingReceiver<web_package::mojom::WebBundleParserFactory>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }
  MockParser* GetCreatedParser() {
    base::RunLoop().RunUntilIdle();
    return parser_.get();
  }
  void DeleteParser() { parser_.reset(); }

 private:
  // web_package::mojom::WebBundleParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
      const absl::optional<GURL>& base_url,
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
      const absl::optional<GURL>& base_url,
      mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source)
      override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
  }

  std::unique_ptr<MockParser> parser_;
  mojo::ReceiverSet<web_package::mojom::WebBundleParserFactory> receivers_;
};

class MockDataSource final : public web_package::mojom::BundleDataSource {
 public:
  explicit MockDataSource(
      mojo::PendingReceiver<web_package::mojom::BundleDataSource> receiver)
      : receiver_(this, std::move(receiver)) {}

  MockDataSource(const MockDataSource&) = delete;
  MockDataSource& operator=(const MockDataSource&) = delete;

 private:
  // Implements web_package::mojom::BundledDataSource.
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {}

  void Length(LengthCallback) override {}

  void IsRandomAccessContext(IsRandomAccessContextCallback) override {}

  mojo::Receiver<web_package::mojom::BundleDataSource> receiver_;
};

}  // namespace

class SafeWebBundleParserTest : public testing::Test {
 public:
  MockFactory* InitializeMockFactory() {
    DCHECK(!factory_);
    factory_ = std::make_unique<MockFactory>();

    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &MockFactory::AddReceiver, base::Unretained(factory_.get())));

    return factory_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockFactory> factory_;
};

TEST_F(SafeWebBundleParserTest, ParseGoldenFile) {
  SafeWebBundleParser parser(/*base_url=*/absl::nullopt);
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));

  base::test::TestFuture<web_package::mojom::BundleMetadataPtr,
                         web_package::mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  parser.ParseMetadata(/*offset=*/absl::nullopt, metadata_future.GetCallback());
  auto [metadata, metadata_error] = metadata_future.Take();
  ASSERT_TRUE(metadata);
  ASSERT_FALSE(metadata_error);
  const auto& requests = metadata->requests;
  ASSERT_EQ(requests.size(), 4u);

  std::map<std::string, web_package::mojom::BundleResponsePtr> responses;
  for (auto& entry : requests) {
    base::test::TestFuture<web_package::mojom::BundleResponsePtr,
                           web_package::mojom::BundleResponseParseErrorPtr>
        response_future;
    parser.ParseResponse(entry.second->offset, entry.second->length,
                         response_future.GetCallback());
    auto [response, response_error] = response_future.Take();
    ASSERT_TRUE(response);
    ASSERT_FALSE(response_error);
    responses.insert({entry.first.spec(), std::move(response)});
  }

  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=utf-8");
  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
  EXPECT_TRUE(responses["https://test.example.org/manifest.webmanifest"]);
  EXPECT_TRUE(responses["https://test.example.org/script.js"]);
}

TEST_F(SafeWebBundleParserTest, OpenInvalidFile) {
  SafeWebBundleParser parser(/*base_url=*/absl::nullopt);
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, parser.OpenFile(base::File()));
}

TEST_F(SafeWebBundleParserTest, CallWithoutOpen) {
  SafeWebBundleParser parser(/*base_url=*/absl::nullopt);
  bool metadata_parsed = false;
  parser.ParseMetadata(
      /*offset=*/absl::nullopt,
      base::BindOnce(
          [](bool* metadata_parsed,
             web_package::mojom::BundleMetadataPtr metadata,
             web_package::mojom::BundleMetadataParseErrorPtr error) {
            EXPECT_FALSE(metadata);
            EXPECT_TRUE(error);
            if (error)
              EXPECT_EQ(kConnectionError, error->message);
            *metadata_parsed = true;
          },
          &metadata_parsed));
  EXPECT_TRUE(metadata_parsed);

  bool response_parsed = false;
  parser.ParseResponse(
      0u, 0u,
      base::BindOnce(
          [](bool* response_parsed,
             web_package::mojom::BundleResponsePtr response,
             web_package::mojom::BundleResponseParseErrorPtr error) {
            EXPECT_FALSE(response);
            EXPECT_TRUE(error);
            if (error)
              EXPECT_EQ(kConnectionError, error->message);
            *response_parsed = true;
          },
          &response_parsed));
  EXPECT_TRUE(response_parsed);
}

TEST_F(SafeWebBundleParserTest, UseMockFactory) {
  SafeWebBundleParser parser(/*base_url=*/absl::nullopt);
  MockFactory* raw_factory = InitializeMockFactory();

  EXPECT_FALSE(raw_factory->GetCreatedParser());
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));
  ASSERT_TRUE(raw_factory->GetCreatedParser());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseIntegrityBlockCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseResponseCalled());

  parser.ParseIntegrityBlock(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseIntegrityBlockCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseResponseCalled());

  parser.ParseMetadata(/*offset=*/absl::nullopt, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseIntegrityBlockCalled());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseResponseCalled());

  parser.ParseResponse(0u, 0u, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseIntegrityBlockCalled());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseResponseCalled());
}

TEST_F(SafeWebBundleParserTest, ConnectionError) {
  auto parser =
      std::make_unique<SafeWebBundleParser>(/*base_url=*/absl::nullopt);
  MockFactory* raw_factory = InitializeMockFactory();

  mojo::PendingRemote<web_package::mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<MockDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver());
  parser->OpenDataSource(std::move(remote_data_source));
  ASSERT_TRUE(raw_factory->GetCreatedParser());

  base::test::TestFuture<void> disconnect_future;
  parser->SetDisconnectCallback(disconnect_future.GetCallback());

  base::test::TestFuture<web_package::mojom::BundleIntegrityBlockPtr,
                         web_package::mojom::BundleIntegrityBlockParseErrorPtr>
      integrity_block_future;
  parser->ParseIntegrityBlock(base::BindLambdaForTesting(
      [&parser, &integrity_block_future](
          web_package::mojom::BundleIntegrityBlockPtr integrity_block,
          web_package::mojom::BundleIntegrityBlockParseErrorPtr
              integrity_block_error) {
        // Synchronously delete the `SafeWebBundleParser` from this callback.
        // This tests that deleting it from a callback that runs as part of
        // `SafeWebBundleParser::OnDisconnect` does not cause a use-after-free.
        parser.reset();
        integrity_block_future.SetValue(std::move(integrity_block),
                                        std::move(integrity_block_error));
      }));
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<web_package::mojom::BundleMetadataPtr,
                         web_package::mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  parser->ParseMetadata(/*offset=*/absl::nullopt,
                        metadata_future.GetCallback());
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<web_package::mojom::BundleResponsePtr,
                         web_package::mojom::BundleResponseParseErrorPtr>
      response_future;
  parser->ParseResponse(0u, 0u, response_future.GetCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseIntegrityBlockCalled());
  EXPECT_FALSE(integrity_block_future.IsReady());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(metadata_future.IsReady());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseResponseCalled());
  EXPECT_FALSE(response_future.IsReady());

  raw_factory->GetCreatedParser()->Disconnect();
  EXPECT_TRUE(disconnect_future.Wait());

  auto [integrity_block, integrity_block_error] = integrity_block_future.Take();
  EXPECT_FALSE(integrity_block);
  EXPECT_EQ(integrity_block_error->type,
            web_package::mojom::BundleParseErrorType::kParserInternalError);
  EXPECT_EQ(integrity_block_error->message, kConnectionError);

  auto [metadata, metadata_error] = metadata_future.Take();
  EXPECT_FALSE(metadata);
  EXPECT_EQ(metadata_error->type,
            web_package::mojom::BundleParseErrorType::kParserInternalError);
  EXPECT_EQ(metadata_error->message, kConnectionError);

  auto [response, response_error] = response_future.Take();
  EXPECT_FALSE(response);
  EXPECT_EQ(response_error->type,
            web_package::mojom::BundleParseErrorType::kParserInternalError);
  EXPECT_EQ(response_error->message, kConnectionError);
}

TEST_F(SafeWebBundleParserTest, ParseSignedWebBundle) {
  SafeWebBundleParser parser(/*base_url=*/absl::nullopt);
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("simple_b2_signed.swbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));

  base::test::TestFuture<web_package::mojom::BundleIntegrityBlockPtr,
                         web_package::mojom::BundleIntegrityBlockParseErrorPtr>
      integrity_block_future;
  parser.ParseIntegrityBlock(integrity_block_future.GetCallback());
  auto [integrity_block, integrity_block_error] = integrity_block_future.Take();
  ASSERT_TRUE(integrity_block);
  ASSERT_FALSE(integrity_block_error);
  ASSERT_EQ(integrity_block->size, 135u);
  ASSERT_EQ(integrity_block->signature_stack.size(), 1u);

  base::test::TestFuture<web_package::mojom::BundleMetadataPtr,
                         web_package::mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  parser.ParseMetadata(integrity_block->size, metadata_future.GetCallback());
  auto [metadata, metadata_error] = metadata_future.Take();
  ASSERT_TRUE(metadata);
  ASSERT_FALSE(metadata_error);
  const auto& requests = metadata->requests;
  ASSERT_EQ(requests.size(), 2u);

  std::map<std::string, web_package::mojom::BundleResponsePtr> responses;
  for (auto& entry : requests) {
    base::test::TestFuture<web_package::mojom::BundleResponsePtr,
                           web_package::mojom::BundleResponseParseErrorPtr>
        response_future;
    parser.ParseResponse(entry.second->offset, entry.second->length,
                         response_future.GetCallback());
    auto [response, response_error] = response_future.Take();
    ASSERT_TRUE(response);
    ASSERT_FALSE(response_error);
    responses.insert({entry.first.spec(), std::move(response)});
  }

  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=UTF-8");
  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
}

TEST_F(SafeWebBundleParserTest, ParseWebBundleWithRelativeUrls) {
  SafeWebBundleParser parser(GURL("https://example.com/foo/"));
  base::File test_file = OpenTestFile(
      base::FilePath(FILE_PATH_LITERAL("mixed_absolute_relative_urls.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));

  base::test::TestFuture<web_package::mojom::BundleMetadataPtr,
                         web_package::mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  parser.ParseMetadata(/*offset=*/absl::nullopt, metadata_future.GetCallback());
  auto [metadata, metadata_error] = metadata_future.Take();
  ASSERT_TRUE(metadata);
  ASSERT_FALSE(metadata_error);

  std::vector<GURL> requests;
  requests.reserve(metadata->requests.size());
  base::ranges::transform(metadata->requests, std::back_inserter(requests),
                          [](const auto& entry) { return entry.first; });
  EXPECT_THAT(requests, UnorderedElementsAreArray(
                            {GURL("https://test.example.org/absolute-url"),
                             GURL("https://example.com/relative-url-1"),
                             GURL("https://example.com/foo/relative-url-2")}));
}

}  // namespace data_decoder
