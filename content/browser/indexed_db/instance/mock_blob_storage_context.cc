// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"

#include <optional>

#include "base/files/important_file_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {

namespace {

class DataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeReader(std::string* data_out, base::OnceClosure done_callback)
      : data_out_(data_out), done_callback_(std::move(done_callback)) {}

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_out_->append(base::as_string_view(data));
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<std::string> data_out_;
  base::OnceClosure done_callback_;
};

class MockBlobReaderClient : public blink::mojom::BlobReaderClient {
 public:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}

  void OnComplete(int32_t status, uint64_t data_length) override {
    result_ = static_cast<net::Error>(status);
  }

  const std::optional<net::Error>& result() const { return result_; }

 private:
  std::optional<net::Error> result_;
};

}  // namespace

MockBlobStorageContext::BlobWrite::BlobWrite() = default;
MockBlobStorageContext::BlobWrite::BlobWrite(BlobWrite&& other) {
  blob = std::move(other.blob);
  path = std::move(other.path);
}

MockBlobStorageContext::BlobWrite::BlobWrite(
    mojo::Remote<::blink::mojom::Blob> blob,
    base::FilePath path)
    : blob(std::move(blob)), path(path) {}

MockBlobStorageContext::BlobWrite::~BlobWrite() = default;

int64_t MockBlobStorageContext::BlobWrite::GetBlobNumber() const {
  int64_t result;
  CHECK(base::StringToInt64(path.BaseName().AsUTF8Unsafe(), &result));
  return result;
}

MockBlobStorageContext::MockBlobStorageContext() = default;

MockBlobStorageContext::~MockBlobStorageContext() = default;

void MockBlobStorageContext::RegisterFromDataItem(
    mojo::PendingReceiver<::blink::mojom::Blob> blob,
    const std::string& uuid,
    storage::mojom::BlobDataItemPtr item) {}

void MockBlobStorageContext::RegisterFromMemory(
    mojo::PendingReceiver<::blink::mojom::Blob> blob,
    const std::string& uuid,
    ::mojo_base::BigBuffer data) {
  NOTREACHED();
}

void MockBlobStorageContext::WriteBlobToFile(
    mojo::PendingRemote<::blink::mojom::Blob> blob,
    const base::FilePath& path,
    bool flush_on_write,
    std::optional<base::Time> last_modified,
    WriteBlobToFileCallback callback) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  mojo::Remote<blink::mojom::Blob> remote(std::move(blob));
  MockBlobReaderClient client;
  mojo::Receiver<blink::mojom::BlobReaderClient> client_receiver(&client);
  remote->ReadAll(std::move(producer_handle),
                  client_receiver.BindNewPipeAndPassRemote());

  base::RunLoop loop;
  std::string received;
  DataPipeReader reader(&received, loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&reader, std::move(consumer_handle));

  client_receiver.FlushForTesting();
  EXPECT_TRUE(client.result().has_value());

  loop.Run();

  writes_.emplace_back(std::move(remote), path);

  if (client.result() == net::Error::OK && write_files_to_disk_) {
    base::ImportantFileWriter::WriteFileAtomically(path, received);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     client.result() == net::Error::OK
                         ? storage::mojom::WriteBlobToFileResult::kSuccess
                         : storage::mojom::WriteBlobToFileResult::kIOError));
}

void MockBlobStorageContext::Clone(
    mojo::PendingReceiver<::storage::mojom::BlobStorageContext> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// static
BlobWriteCallback MockBlobStorageContext::CreateBlobWriteCallback(
    bool* succeeded,
    base::OnceClosure on_done) {
  *succeeded = false;
  return base::BindOnce(
      [](bool* succeeded, base::OnceClosure on_done,
         StatusOr<BlobWriteResult> result) {
        *succeeded = result.has_value();
        std::move(on_done).Run();
        return Status::OK();
      },
      succeeded, std::move(on_done));
}

void MockBlobStorageContext::ClearWrites() {
  writes_.clear();
}

}  // namespace content::indexed_db
