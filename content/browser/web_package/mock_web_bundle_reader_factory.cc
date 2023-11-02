// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_web_bundle_reader_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "components/web_package/test_support/mock_web_bundle_parser_factory.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/filename_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockWebBundleReaderFactoryImpl final : public MockWebBundleReaderFactory {
 public:
  MockWebBundleReaderFactoryImpl() : MockWebBundleReaderFactory() {}

  MockWebBundleReaderFactoryImpl(const MockWebBundleReaderFactoryImpl&) =
      delete;
  MockWebBundleReaderFactoryImpl& operator=(
      const MockWebBundleReaderFactoryImpl&) = delete;

  ~MockWebBundleReaderFactoryImpl() override {
    EXPECT_TRUE(!temp_dir_.IsValid() || temp_dir_.Delete())
        << temp_dir_.GetPath();
  }

  scoped_refptr<WebBundleReader> CreateReader(
      const std::string& test_file_data) override {
    DCHECK(!factory_);
    if (!temp_dir_.CreateUniqueTempDir() ||
        !CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path_) ||
        (test_file_data.size() !=
         static_cast<size_t>(base::WriteFile(
             temp_file_path_, test_file_data.data(), test_file_data.size())))) {
      return nullptr;
    }

    auto reader = base::MakeRefCounted<WebBundleReader>(
        WebBundleSource::MaybeCreateFromTrustedFileUrl(
            net::FilePathToFileURL(temp_file_path_)));

    factory_ = std::make_unique<web_package::MockWebBundleParserFactory>();
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &web_package::MockWebBundleParserFactory::AddReceiver,
            base::Unretained(factory_.get())));
    return reader;
  }

  void ReadAndFullfillMetadata(
      WebBundleReader* reader,
      web_package::mojom::BundleMetadataPtr metadata,
      WebBundleReader::MetadataCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::test::TestFuture<web_package::mojom::BundleMetadataParseErrorPtr>
        future;
    reader->ReadMetadata(future.GetCallback());
    factory_->RunMetadataCallback(/*expected_metadata_offset=*/-1,
                                  std::move(metadata));
    std::move(callback).Run(future.Take());
  }

  void ReadAndFullfillResponse(
      WebBundleReader* reader,
      const network::ResourceRequest& resource_request,
      web_package::mojom::BundleResponseLocationPtr expected_parse_args,
      web_package::mojom::BundleResponsePtr response,
      WebBundleReader::ResponseCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::test::TestFuture<web_package::mojom::BundleResponsePtr,
                           web_package::mojom::BundleResponseParseErrorPtr>
        future;
    reader->ReadResponse(resource_request, future.GetCallback());
    factory_->RunResponseCallback(std::move(expected_parse_args),
                                  std::move(response));
    auto [bundle_response, error] = future.Take();
    std::move(callback).Run(std::move(bundle_response), std::move(error));
  }

  void FullfillResponse(
      web_package::mojom::BundleResponseLocationPtr expected_parse_args,
      web_package::mojom::BundleResponsePtr response) override {
    ASSERT_TRUE(factory_);

    factory_->RunResponseCallback(std::move(expected_parse_args),
                                  std::move(response));
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::string test_file_data_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  std::unique_ptr<web_package::MockWebBundleParserFactory> factory_;
};

}  // namespace

// static
std::unique_ptr<MockWebBundleReaderFactory>
MockWebBundleReaderFactory::Create() {
  return std::make_unique<MockWebBundleReaderFactoryImpl>();
}

}  // namespace content
