// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/blob_writer.h"

#include <tuple>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace content::indexed_db::sqlite {

// static
std::unique_ptr<BlobWriter> BlobWriter::WriteBlobIntoDatabase(
    IndexedDBExternalObject& external_object,
    base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
        fetch_blob_chunk,
    base::OnceCallback<void(bool)> on_complete) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      CreateDataPipe(/*options=*/nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    return nullptr;
  }

  external_object.remote()->ReadAll(std::move(producer_handle),
                                    mojo::NullRemote());
  auto sink = base::WrapUnique(
      new BlobWriter(std::move(fetch_blob_chunk), std::move(on_complete)));
  sink->Start(std::move(consumer_handle));
  return sink;
}

BlobWriter::BlobWriter(
    base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
        fetch_blob_chunk,
    base::OnceCallback<void(bool)> on_complete)
    : fetch_blob_chunk_(std::move(fetch_blob_chunk)),
      on_complete_(std::move(on_complete)) {}

BlobWriter::~BlobWriter() = default;

void BlobWriter::Start(mojo::ScopedDataPipeConsumerHandle consumer_handle) {
  if (!(blob_chunk_ = fetch_blob_chunk_.Run(next_blob_chunk_idx_++))) {
    OnSqlError();
    return;
  }
  drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void BlobWriter::OnDataAvailable(base::span<const uint8_t> data) {
  if (!blob_chunk_) {
    return;
  }

  // Loop because the data may span multiple chunks.
  for (base::span<const uint8_t> data_left_to_write = data;
       !data_left_to_write.empty();) {
    if (bytes_written_this_chunk_ == blob_chunk_->GetSize()) {
      blob_chunk_ = fetch_blob_chunk_.Run(next_blob_chunk_idx_++);
      if (!blob_chunk_) {
        OnSqlError();
        return;
      }
      bytes_written_this_chunk_ = 0;
    }

    size_t space_remaining_in_blob = base::checked_cast<size_t>(
        blob_chunk_->GetSize() - bytes_written_this_chunk_);
    base::span<const uint8_t> bytes_to_write;
    std::tie(bytes_to_write, data_left_to_write) = data_left_to_write.split_at(
        std::min(data_left_to_write.size(), space_remaining_in_blob));

    if (blob_chunk_->Write(bytes_written_this_chunk_, bytes_to_write)) {
      bytes_written_this_chunk_ += bytes_to_write.size();
    } else {
      OnSqlError();
      return;
    }
  }
}

void BlobWriter::OnDataComplete() {
  if (blob_chunk_ && on_complete_) {
    std::move(on_complete_).Run(/*success=*/true);
  }
}

void BlobWriter::OnSqlError() {
  blob_chunk_.reset();
  // Reporting an error deletes `this`, but `drainer_` doesn't like being
  // deleted inside `OnDataAvailable`. This also makes sure `on_complete_` isn't
  // run synchronously during `Start()`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_complete_), /*success=*/false));
}

}  // namespace content::indexed_db::sqlite
