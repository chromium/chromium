// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/mock_blob_storage_context.h"

#include "base/files/important_file_writer.h"
#include "base/strings/string_number_conversions.h"

namespace content::indexed_db {

MockBlobStorageContext::BlobWrite::BlobWrite() = default;
MockBlobStorageContext::BlobWrite::BlobWrite(BlobWrite&& other) {
  blob = std::move(other.blob);
  path = std::move(other.path);
}

MockBlobStorageContext::BlobWrite::BlobWrite(
    mojo::PendingRemote<::blink::mojom::Blob> blob,
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
  writes_.emplace_back(std::move(blob), path);

  if (write_files_to_disk_) {
    base::ImportantFileWriter::WriteFileAtomically(path, "fake contents");
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     storage::mojom::WriteBlobToFileResult::kSuccess));
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
      [](bool* succeeded, base::OnceClosure on_done, BlobWriteResult result,
         storage::mojom::WriteBlobToFileResult error) {
        switch (result) {
          case BlobWriteResult::kFailure:
            NOTREACHED();
          case BlobWriteResult::kRunPhaseTwoAsync:
          case BlobWriteResult::kRunPhaseTwoAndReturnResult:
            DCHECK_EQ(error, storage::mojom::WriteBlobToFileResult::kSuccess);
            *succeeded = true;
            break;
        }
        std::move(on_done).Run();
        return Status::OK();
      },
      succeeded, std::move(on_done));
}

void MockBlobStorageContext::ClearWrites() {
  writes_.clear();
}

}  // namespace content::indexed_db
