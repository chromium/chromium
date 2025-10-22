// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_BLOB_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_BLOB_STORAGE_CONTEXT_H_

#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content::indexed_db {

class MockBlobStorageContext : public ::storage::mojom::BlobStorageContext {
 public:
  struct BlobWrite {
    BlobWrite();
    BlobWrite(BlobWrite&& other);
    BlobWrite(mojo::Remote<::blink::mojom::Blob> blob, base::FilePath path);
    ~BlobWrite();

    int64_t GetBlobNumber() const;

    mojo::Remote<::blink::mojom::Blob> blob;
    base::FilePath path;
  };

  MockBlobStorageContext();
  ~MockBlobStorageContext() override;

  void RegisterFromDataItem(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                            const std::string& uuid,
                            storage::mojom::BlobDataItemPtr item) override;
  void RegisterFromMemory(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                          const std::string& uuid,
                          ::mojo_base::BigBuffer data) override;
  void WriteBlobToFile(mojo::PendingRemote<::blink::mojom::Blob> blob,
                       const base::FilePath& path,
                       bool flush_on_write,
                       std::optional<base::Time> last_modified,
                       WriteBlobToFileCallback callback) override;
  void Clone(mojo::PendingReceiver<::storage::mojom::BlobStorageContext>
                 receiver) override;

  static BlobWriteCallback CreateBlobWriteCallback(
      bool* succeeded,
      base::OnceClosure on_done = base::DoNothing());

  void ClearWrites();

  const std::vector<BlobWrite>& writes() const { return writes_; }

  // If true, writes a fake file for each blob file to disk.
  // The contents are bogus, but the files will exist.
  void SetWriteFilesToDisk(bool write) { write_files_to_disk_ = write; }

 private:
  std::vector<BlobWrite> writes_;
  bool write_files_to_disk_ = false;
  mojo::ReceiverSet<::storage::mojom::BlobStorageContext> receivers_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_BLOB_STORAGE_CONTEXT_H_
