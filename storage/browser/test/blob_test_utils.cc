// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/blob_test_utils.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"

namespace storage {

namespace {

class MemoryBlobDataItemReader : public mojom::BlobDataItemReader {
 public:
  explicit MemoryBlobDataItemReader(std::string data)
      : data_(std::move(data)) {}

  MemoryBlobDataItemReader(const MemoryBlobDataItemReader&) = delete;
  MemoryBlobDataItemReader& operator=(const MemoryBlobDataItemReader&) = delete;

  ~MemoryBlobDataItemReader() override = default;

  void Read(uint64_t offset,
            uint64_t length,
            mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    if (offset > data_.size()) {
      std::move(callback).Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }

    const size_t read_offset = static_cast<size_t>(offset);
    const size_t read_length = static_cast<size_t>(
        std::min<uint64_t>(length, data_.size() - read_offset));
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
    const bool success = mojo::BlockingCopyFromString(
        data_.substr(read_offset, read_length), pipe);
    std::move(callback).Run(success ? net::OK : net::ERR_FAILED);
  }

 private:
  const std::string data_;
};

// Helper class to copy a blob to a string.
class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  explicit DataPipeDrainerClient(std::string* output) : output_(output) {}
  void Run() { run_loop_.Run(); }

  void OnDataAvailable(base::span<const uint8_t> data) override {
    output_->append(base::as_string_view(data));
  }
  void OnDataComplete() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<std::string> output_;
};

}  // namespace

std::string BlobToString(blink::mojom::Blob* blob) {
  std::string output;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(/*options=*/nullptr, producer, consumer));
  blob->ReadAll(std::move(producer), mojo::NullRemote());
  DataPipeDrainerClient client(&output);
  mojo::DataPipeDrainer drainer(&client, std::move(consumer));
  client.Run();
  return output;
}

mojom::BlobDataItemPtr CreateBlobDataItemFromString(std::string body_data) {
  mojom::BlobDataItemPtr item = mojom::BlobDataItem::New();
  item->size = body_data.size();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MemoryBlobDataItemReader>(std::move(body_data)),
      item->reader.InitWithNewPipeAndPassReceiver());
  return item;
}

void RegisterBlobWithContents(mojom::BlobStorageContext& blob_storage_context,
                              mojo::PendingReceiver<blink::mojom::Blob> blob,
                              const std::string& uuid,
                              base::span<const uint8_t> data) {
  blob_storage_context.RegisterFromDataItem(
      std::move(blob), uuid,
      CreateBlobDataItemFromString(std::string(base::as_string_view(data))));
}

}  // namespace storage
