// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/bundled_exchanges_parser_factory.h"

#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "services/data_decoder/bundled_exchanges_parser.h"

namespace data_decoder {

namespace {

class FileDataSource final : public mojom::BundleDataSource {
 public:
  FileDataSource(mojo::PendingReceiver<mojom::BundleDataSource> receiver,
                 base::File file)
      : receiver_(this, std::move(receiver)), file_(std::move(file)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &base::DeletePointer<FileDataSource>, base::Unretained(this)));
  }

 private:
  // Implements mojom::BundleDataSource.
  void GetSize(GetSizeCallback callback) override {
    std::move(callback).Run(file_.GetLength());
  }
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    std::vector<uint8_t> buf(length);
    uint64_t bytes =
        file_.Read(offset, reinterpret_cast<char*>(buf.data()), length);
    if (bytes != length)
      std::move(callback).Run(base::nullopt);
    else
      std::move(callback).Run(std::move(buf));
  }

  mojo::Receiver<mojom::BundleDataSource> receiver_;
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace

BundledExchangesParserFactory::BundledExchangesParserFactory() = default;

BundledExchangesParserFactory::~BundledExchangesParserFactory() = default;

std::unique_ptr<mojom::BundleDataSource>
BundledExchangesParserFactory::CreateFileDataSourceForTesting(
    mojo::PendingReceiver<mojom::BundleDataSource> receiver,
    base::File file) {
  return std::make_unique<FileDataSource>(std::move(receiver), std::move(file));
}

void BundledExchangesParserFactory::GetParserForFile(
    mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
    base::File file) {
  mojo::PendingRemote<mojom::BundleDataSource> remote_data_source;
  auto data_source = std::make_unique<FileDataSource>(
      remote_data_source.InitWithNewPipeAndPassReceiver(), std::move(file));
  GetParserForDataSource(std::move(receiver), std::move(remote_data_source));

  // |data_source| will be destructed on |remote_data_source| destruction.
  data_source.release();
}

void BundledExchangesParserFactory::GetParserForDataSource(
    mojo::PendingReceiver<mojom::BundledExchangesParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source) {
  auto parser = std::make_unique<BundledExchangesParser>(
      std::move(receiver), std::move(data_source));

  // |parser| will be destructed on remote mojo ends' disconnection.
  parser.release();
}

}  // namespace data_decoder
