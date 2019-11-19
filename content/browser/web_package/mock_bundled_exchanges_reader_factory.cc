// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_bundled_exchanges_reader_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/filename_util.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockParser final : public data_decoder::mojom::BundledExchangesParser {
 public:
  MockParser(mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
                 receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockParser() override = default;

  void RunMetadataCallback(data_decoder::mojom::BundleMetadataPtr metadata) {
    std::move(metadata_callback_).Run(std::move(metadata), nullptr);
  }
  void RunResponseCallback(data_decoder::mojom::BundleResponsePtr response) {
    std::move(response_callback_).Run(std::move(response), nullptr);
  }

  void WaitUntilParseMetadataCalled(base::OnceClosure closure) {
    if (metadata_callback_.is_null())
      wait_parse_metadata_callback_ = std::move(closure);
    else
      std::move(closure).Run();
  }
  void WaitUntilParseResponseCalled(base::OnceClosure closure) {
    if (response_callback_.is_null())
      wait_parse_response_callback_ = std::move(closure);
    else
      std::move(closure).Run();
  }

 private:
  // data_decoder::mojom::BundledExchangesParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override {
    metadata_callback_ = std::move(callback);
    if (!wait_parse_metadata_callback_.is_null())
      std::move(wait_parse_metadata_callback_).Run();
  }
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override {
    response_callback_ = std::move(callback);
    if (!wait_parse_response_callback_.is_null())
      std::move(wait_parse_response_callback_).Run();
  }

  mojo::Receiver<data_decoder::mojom::BundledExchangesParser> receiver_;

  ParseMetadataCallback metadata_callback_;
  ParseResponseCallback response_callback_;
  base::OnceClosure wait_parse_metadata_callback_;
  base::OnceClosure wait_parse_response_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockParser);
};

class MockParserFactory final
    : public data_decoder::mojom::BundledExchangesParserFactory {
 public:
  MockParserFactory() {}
  ~MockParserFactory() override = default;

  void WaitUntilParseMetadataCalled(base::OnceClosure closure) {
    if (parser_)
      parser_->WaitUntilParseMetadataCalled(std::move(closure));
    else
      wait_parse_metadata_callback_ = std::move(closure);
  }
  void WaitUntilParseResponseCalled(base::OnceClosure closure) {
    if (parser_)
      parser_->WaitUntilParseResponseCalled(std::move(closure));
    else
      wait_parse_response_callback_ = std::move(closure);
  }

  void RunMetadataCallback(data_decoder::mojom::BundleMetadataPtr metadata) {
    base::RunLoop run_loop;
    WaitUntilParseMetadataCalled(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(parser_);
    parser_->RunMetadataCallback(std::move(metadata));
  }
  void RunResponseCallback(data_decoder::mojom::BundleResponsePtr response) {
    base::RunLoop run_loop;
    WaitUntilParseResponseCalled(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(parser_);
    parser_->RunResponseCallback(std::move(response));
  }

 private:
  // data_decoder::mojom::BundledExchangesParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
          receiver,
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
    if (!wait_parse_metadata_callback_.is_null()) {
      parser_->WaitUntilParseMetadataCalled(
          std::move(wait_parse_metadata_callback_));
    }
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
          receiver,
      mojo::PendingRemote<data_decoder::mojom::BundleDataSource> data_source)
      override {
    NOTREACHED();
  }

  std::unique_ptr<MockParser> parser_;
  base::OnceClosure wait_parse_metadata_callback_;
  base::OnceClosure wait_parse_response_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockParserFactory);
};

class MockBundledExchangesReaderFactoryImpl final
    : public MockBundledExchangesReaderFactory {
 public:
  MockBundledExchangesReaderFactoryImpl()
      : MockBundledExchangesReaderFactory() {}
  ~MockBundledExchangesReaderFactoryImpl() override = default;

  scoped_refptr<BundledExchangesReader> CreateReader(
      const std::string& test_file_data) override {
    if (!temp_dir_.CreateUniqueTempDir() ||
        !CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path_) ||
        (test_file_data.size() !=
         static_cast<size_t>(base::WriteFile(
             temp_file_path_, test_file_data.data(), test_file_data.size())))) {
      return nullptr;
    }

    auto reader = base::MakeRefCounted<BundledExchangesReader>(
        BundledExchangesSource::MaybeCreateFromTrustedFileUrl(
            net::FilePathToFileURL(temp_file_path_)));

    std::unique_ptr<MockParserFactory> factory =
        std::make_unique<MockParserFactory>();
    factory_ = factory.get();
    mojo::Remote<data_decoder::mojom::BundledExchangesParserFactory> remote;
    mojo::MakeSelfOwnedReceiver(std::move(factory),
                                remote.BindNewPipeAndPassReceiver());
    reader->SetBundledExchangesParserFactoryForTesting(std::move(remote));
    return reader;
  }

  void ReadAndFullfillMetadata(
      BundledExchangesReader* reader,
      data_decoder::mojom::BundleMetadataPtr metadata,
      BundledExchangesReader::MetadataCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::RunLoop run_loop;
    reader->ReadMetadata(base::BindOnce(
        [](base::Closure quit_closure,
           BundledExchangesReader::MetadataCallback callback,
           data_decoder::mojom::BundleMetadataParseErrorPtr error) {
          std::move(callback).Run(std::move(error));
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), std::move(callback)));

    factory_->RunMetadataCallback(std::move(metadata));
    run_loop.Run();
  }

  void ReadAndFullfillResponse(
      BundledExchangesReader* reader,
      const GURL& url,
      data_decoder::mojom::BundleResponsePtr response,
      BundledExchangesReader::ResponseCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::RunLoop run_loop;
    reader->ReadResponse(
        url, base::BindOnce(
                 [](base::Closure quit_closure,
                    BundledExchangesReader::ResponseCallback callback,
                    data_decoder::mojom::BundleResponsePtr response,
                    data_decoder::mojom::BundleResponseParseErrorPtr error) {
                   std::move(callback).Run(std::move(response),
                                           std::move(error));
                   std::move(quit_closure).Run();
                 },
                 run_loop.QuitClosure(), std::move(callback)));

    factory_->RunResponseCallback(std::move(response));
    run_loop.Run();
  }

  void FullfillResponse(
      data_decoder::mojom::BundleResponsePtr response,
      BundledExchangesReader::ResponseCallback callback) override {
    ASSERT_TRUE(factory_);

    factory_->RunResponseCallback(std::move(response));
  }

 private:
  std::string test_file_data_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  MockParserFactory* factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockBundledExchangesReaderFactoryImpl);
};

}  // namespace

// static
std::unique_ptr<MockBundledExchangesReaderFactory>
MockBundledExchangesReaderFactory::Create() {
  return std::make_unique<MockBundledExchangesReaderFactoryImpl>();
}

}  // namespace content
