// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "sql/streaming_blob_handle.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content::indexed_db {

class IndexedDBExternalObject;

namespace sqlite {

// This class reads all the data from a mojo Blob and writes it into the
// provided SQL address. It is owned by the DatabaseConnection.
class BlobWriter : public mojo::DataPipeDrainer::Client,
                   public blink::mojom::BlobReaderClient {
 public:
  // `on_complete` will always be called and will not be called synchronously.
  static std::unique_ptr<BlobWriter> WriteBlobIntoDatabase(
      // Contains a mojo Blob connection from which bytes are read.
      IndexedDBExternalObject& external_object,
      base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
          fetch_blob_chunk,
      base::OnceCallback<void(/*success=*/bool)> on_complete);

  ~BlobWriter() override;

 private:
  BlobWriter(
      base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
          fetch_blob_chunk,
      base::OnceCallback<void(/*success=*/bool)> on_complete);

  void Start(IndexedDBExternalObject& external_object);

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // blink::mojom::BlobReaderClient
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  // This override is necessary only because DataPipeDrainer::Client does not
  // signal failure.
  void OnComplete(int32_t status, uint64_t data_length) override;

  // Invokes `on_complete_` only if both the DataPipeDrainer::Client and
  // BlobReaderClient interfaces have signaled completion.
  void MaybeComplete();

  // Called after various types of errors, such as if `fetch_blob_chunk_` fails
  // to return a handle or fails to write bytes, a data pipe cannot be opened,
  // etc.
  void OnError();

  mojo::Receiver<blink::mojom::BlobReaderClient> blob_reader_receiver_{this};
  // Null until OnComplete() has been called, then holds the status passed to
  // that method.
  std::optional<int32_t> final_status_;

  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  // True after OnDataComplete() has been called.
  bool data_complete_ = false;

  // Used to retrieve the next blob handle after the current one has been
  // filled. The argument is the index of the chunk. See `overflow_blob_chunks`
  // table in `DatabaseConnection` for information about blob chunking.
  const base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
      fetch_blob_chunk_;
  size_t next_blob_chunk_idx_ = 0;

  // The current handle for streaming bytes into. This is a cached result of
  // `fetch_blob_chunk_`.
  std::optional<sql::StreamingBlobHandle> blob_chunk_;

  // The position in the blob for the next write.
  int bytes_written_this_chunk_ = 0;

  // Called when done, with the parameter indicating success.
  base::OnceCallback<void(/*success=*/bool)> on_complete_;

  base::WeakPtrFactory<BlobWriter> weak_factory_{this};
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BLOB_WRITER_H_
