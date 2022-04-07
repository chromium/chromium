// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_web_bundle_parser.h"

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace data_decoder {

namespace {

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

    bool IsParseMetadataCalled() { return !metadata_callback_.is_null(); }
    bool IsParseResponseCalled() { return !response_callback_.is_null(); }

    void Disconnect() { receiver_.reset(); }

   private:
    // web_package::mojom::WebBundleParser implementation.
    void ParseMetadata(ParseMetadataCallback callback) override {
      metadata_callback_ = std::move(callback);
    }
    void ParseResponse(uint64_t response_offset,
                       uint64_t response_length,
                       ParseResponseCallback callback) override {
      response_callback_ = std::move(callback);
    }

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
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<web_package::mojom::WebBundleParser> receiver,
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
  SafeWebBundleParser parser;
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));

  base::test::TestFuture<web_package::mojom::BundleMetadataPtr,
                         web_package::mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  parser.ParseMetadata(metadata_future.GetCallback());
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
  SafeWebBundleParser parser;
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, parser.OpenFile(base::File()));
}

TEST_F(SafeWebBundleParserTest, CallWithoutOpen) {
  SafeWebBundleParser parser;
  bool metadata_parsed = false;
  parser.ParseMetadata(base::BindOnce(
      [](bool* metadata_parsed, web_package::mojom::BundleMetadataPtr metadata,
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
  SafeWebBundleParser parser;
  MockFactory* raw_factory = InitializeMockFactory();

  EXPECT_FALSE(raw_factory->GetCreatedParser());
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));
  ASSERT_TRUE(raw_factory->GetCreatedParser());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseResponseCalled());

  parser.ParseMetadata(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_FALSE(raw_factory->GetCreatedParser()->IsParseResponseCalled());

  parser.ParseResponse(0u, 0u, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseResponseCalled());
}

TEST_F(SafeWebBundleParserTest, ConnectionError) {
  SafeWebBundleParser parser;
  MockFactory* raw_factory = InitializeMockFactory();

  mojo::PendingRemote<web_package::mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<MockDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver());
  parser.OpenDataSource(std::move(remote_data_source));
  ASSERT_TRUE(raw_factory->GetCreatedParser());

  base::RunLoop run_loop;
  bool parsed = false;
  parser.ParseMetadata(base::BindOnce(
      [](base::OnceClosure quit_closure, bool* parsed,
         web_package::mojom::BundleMetadataPtr metadata,
         web_package::mojom::BundleMetadataParseErrorPtr error) {
        EXPECT_FALSE(metadata);
        EXPECT_TRUE(error);
        if (error)
          EXPECT_EQ(kConnectionError, error->message);
        *parsed = true;
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &parsed));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(parsed);
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());

  raw_factory->GetCreatedParser()->Disconnect();
  run_loop.Run();
  ASSERT_TRUE(parsed);

  // Passed callback is called by SafeWebBundleParser on the interface
  // disconnection, but remote parser still holds the proxy callback.
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
}

}  // namespace data_decoder
