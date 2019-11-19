// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_bundled_exchanges_parser.h"

#include <memory>
#include <string>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {

constexpr char kConnectionError[] =
    "Cannot connect to the remote parser service";

base::File OpenTestFile(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(base::FilePath(
      FILE_PATH_LITERAL("services/test/data/bundled_exchanges")));
  test_data_dir = test_data_dir.Append(path);
  return base::File(test_data_dir,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

class MockFactory final : public mojom::BundledExchangesParserFactory {
 public:
  class MockParser final : public mojom::BundledExchangesParser {
   public:
    MockParser(mojo::PendingReceiver<mojom::BundledExchangesParser> receiver)
        : receiver_(this, std::move(receiver)) {}

    bool IsParseMetadataCalled() { return !metadata_callback_.is_null(); }
    bool IsParseResponseCalled() { return !response_callback_.is_null(); }

    void Disconnect() { receiver_.reset(); }

   private:
    // mojom::BundledExchangesParser implementation.
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
    mojo::Receiver<mojom::BundledExchangesParser> receiver_;

    DISALLOW_COPY_AND_ASSIGN(MockParser);
  };

  MockFactory() {}
  MockParser* GetCreatedParser() {
    base::RunLoop().RunUntilIdle();
    return parser_.get();
  }
  void DeleteParser() { parser_.reset(); }

 private:
  // mojom::BundledExchangesParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
  }

  std::unique_ptr<MockParser> parser_;

  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};

class MockDataSource final : public mojom::BundleDataSource {
 public:
  MockDataSource(mojo::PendingReceiver<mojom::BundleDataSource> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // Implements mojom::BundledDataSource.
  void GetSize(GetSizeCallback callback) override {}
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {}

  mojo::Receiver<mojom::BundleDataSource> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

}  // namespace

class SafeBundledExchangesParserTest : public testing::Test {
 public:
  MockFactory* InitializeMockFactory(SafeBundledExchangesParser* parser) {
    std::unique_ptr<MockFactory> factory = std::make_unique<MockFactory>();
    MockFactory* raw_factory = factory.get();
    mojo::Remote<mojom::BundledExchangesParserFactory> remote;
    mojo::MakeSelfOwnedReceiver(std::move(factory),
                                remote.BindNewPipeAndPassReceiver());
    parser->SetBundledExchangesParserFactoryForTesting(std::move(remote));
    return raw_factory;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(SafeBundledExchangesParserTest, ParseGoldenFile) {
  SafeBundledExchangesParser parser;
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));
  ASSERT_EQ(base::File::FILE_OK, parser.OpenFile(std::move(test_file)));

  mojom::BundleMetadataPtr metadata_result;
  {
    base::RunLoop run_loop;
    parser.ParseMetadata(base::BindOnce(
        [](base::Closure quit_closure,
           mojom::BundleMetadataPtr* metadata_result,
           mojom::BundleMetadataPtr metadata,
           mojom::BundleMetadataParseErrorPtr error) {
          EXPECT_TRUE(metadata);
          EXPECT_FALSE(error);
          if (metadata)
            *metadata_result = std::move(metadata);
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &metadata_result));
    run_loop.Run();
  }
  ASSERT_TRUE(metadata_result);
  const auto& requests = metadata_result->requests;
  ASSERT_EQ(requests.size(), 4u);

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (auto& entry : requests) {
    base::RunLoop run_loop;
    parser.ParseResponse(
        entry.second->response_locations[0]->offset,
        entry.second->response_locations[0]->length,
        base::BindOnce(
            [](base::Closure quit_closure, const std::string url,
               std::map<std::string, mojom::BundleResponsePtr>* responses,
               mojom::BundleResponsePtr response,
               mojom::BundleResponseParseErrorPtr error) {
              EXPECT_TRUE(response);
              EXPECT_FALSE(error);
              if (response)
                responses->insert({url, std::move(response)});
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), entry.first.spec(), &responses));
    run_loop.Run();
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

TEST_F(SafeBundledExchangesParserTest, OpenInvalidFile) {
  SafeBundledExchangesParser parser;
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, parser.OpenFile(base::File()));
}

TEST_F(SafeBundledExchangesParserTest, CallWithoutOpen) {
  SafeBundledExchangesParser parser;
  bool metadata_parsed = false;
  parser.ParseMetadata(base::BindOnce(
      [](bool* metadata_parsed, mojom::BundleMetadataPtr metadata,
         mojom::BundleMetadataParseErrorPtr error) {
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
          [](bool* response_parsed, mojom::BundleResponsePtr response,
             mojom::BundleResponseParseErrorPtr error) {
            EXPECT_FALSE(response);
            EXPECT_TRUE(error);
            if (error)
              EXPECT_EQ(kConnectionError, error->message);
            *response_parsed = true;
          },
          &response_parsed));
  EXPECT_TRUE(response_parsed);
}

TEST_F(SafeBundledExchangesParserTest, UseMockFactory) {
  SafeBundledExchangesParser parser;
  MockFactory* raw_factory = InitializeMockFactory(&parser);

  EXPECT_FALSE(raw_factory->GetCreatedParser());
  base::File test_file =
      OpenTestFile(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));
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

TEST_F(SafeBundledExchangesParserTest, ConnectionError) {
  SafeBundledExchangesParser parser;
  MockFactory* raw_factory = InitializeMockFactory(&parser);

  mojo::PendingRemote<mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<MockDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver());
  parser.OpenDataSource(std::move(remote_data_source));
  ASSERT_TRUE(raw_factory->GetCreatedParser());

  base::RunLoop run_loop;
  bool parsed = false;
  parser.ParseMetadata(base::BindOnce(
      [](base::Closure quit_closure, bool* parsed,
         mojom::BundleMetadataPtr metadata,
         mojom::BundleMetadataParseErrorPtr error) {
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

  // Passed callback is called by SafeBundledExchangesParser on the interface
  // disconnection, but remote parser still holds the proxy callback.
  EXPECT_TRUE(raw_factory->GetCreatedParser()->IsParseMetadataCalled());
}

}  // namespace data_decoder
