// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_

#include <memory>

#include "base/functional/callback.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "sql/streaming_blob_handle.h"

namespace content::indexed_db {

class IndexedDBExternalObject;

namespace sqlite {

// This class reads all the data from a mojo Blob and writes it into the
// provided SQL address. It is owned by the DatabaseConnection.
class BlobWriter : public mojo::DataPipeDrainer::Client {
 public:
  // Will return null if there's a synchronous error (a mojo pipe couldn't be
  // created due to insufficient resources), in which case `on_complete` is
  // never called.
  static std::unique_ptr<BlobWriter> WriteBlobIntoDatabase(
      // Contains a mojo Blob connection from which bytes are read.
      IndexedDBExternalObject& external_object,
      // The destination for the bytes.
      sql::StreamingBlobHandle blob_handle,
      base::OnceCallback<void(/*success=*/bool)> on_complete);

  ~BlobWriter() override;

 private:
  BlobWriter(sql::StreamingBlobHandle blob_handle,
             base::OnceCallback<void(/*success=*/bool)> on_complete);

  void Start(mojo::ScopedDataPipeConsumerHandle consumer_handle);

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // The position in the blob for the next write.
  size_t bytes_written_so_far_ = 0;

  // Will be set to null if an error has occurred when attempting to write into
  // it.
  std::optional<sql::StreamingBlobHandle> target_;

  std::unique_ptr<mojo::DataPipeDrainer> drainer_;

  // Called when done, with the parameter indicating success.
  base::OnceCallback<void(/*success=*/bool)> on_complete_;

  base::WeakPtrFactory<BlobWriter> weak_factory_{this};
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_
