// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"

#include <optional>

#include "base/files/important_file_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "storage/browser/test/fake_blob.h"
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
    storage::mojom::BlobDataItemPtr item) {
  // Create a fake blob and bind it to the receiver. This is needed so that
  // when the blob is read back, the BlobReader has a valid registry_blob_.
  auto fake_blob = std::make_unique<storage::FakeBlob>(uuid);
  // Don't set a body - this blob is just a placeholder for the registry.
  // The actual content will be read via the BlobDataItemReader interface.
  mojo::MakeSelfOwnedReceiver(std::move(fake_blob), std::move(blob));
}

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
    uint64_t expected_size,
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

  storage::mojom::WriteBlobToFileResult result;
  if (client.result() != net::Error::OK) {
    result = storage::mojom::WriteBlobToFileResult::kIOError;
  } else if (received.size() > expected_size) {
    result = storage::mojom::WriteBlobToFileResult::kInvalidBlob;
  } else {
    if (write_files_to_disk_) {
      base::ImportantFileWriter::WriteFileAtomically(path, received);
    }
    result = storage::mojom::WriteBlobToFileResult::kSuccess;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
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
      [](bool* succeeded, base::OnceClosure on_done, Status result) {
        *succeeded = result.ok();
        std::move(on_done).Run();
      },
      succeeded, std::move(on_done));
}

void MockBlobStorageContext::ClearWrites() {
  writes_.clear();
}

}  // namespace content::indexed_db
