// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/blob_writer.h"

#include <tuple>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"

namespace content::indexed_db::sqlite {

// static
std::unique_ptr<BlobWriter> BlobWriter::WriteBlobIntoDatabase(
    IndexedDBExternalObject& external_object,
    base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
        fetch_blob_chunk,
    base::OnceCallback<void(bool)> on_complete) {
  auto sink = base::WrapUnique(
      new BlobWriter(std::move(fetch_blob_chunk), std::move(on_complete)));
  sink->Start(external_object);
  return sink;
}

BlobWriter::BlobWriter(
    base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
        fetch_blob_chunk,
    base::OnceCallback<void(bool)> on_complete)
    : fetch_blob_chunk_(std::move(fetch_blob_chunk)),
      on_complete_(std::move(on_complete)) {}

BlobWriter::~BlobWriter() = default;

void BlobWriter::Start(IndexedDBExternalObject& external_object) {
  if (!(blob_chunk_ = fetch_blob_chunk_.Run(next_blob_chunk_idx_++))) {
    OnError();
    return;
  }

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      CreateDataPipe(/*options=*/nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    OnError();
    return;
  }

  external_object.remote()->ReadAll(
      std::move(producer_handle),
      blob_reader_receiver_.BindNewPipeAndPassRemote());

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
        OnError();
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
      OnError();
      return;
    }
  }
}

void BlobWriter::OnDataComplete() {
  data_complete_ = true;
  MaybeComplete();
}

void BlobWriter::OnComplete(int32_t status, uint64_t data_length) {
  final_status_ = status;
  MaybeComplete();
}

void BlobWriter::MaybeComplete() {
  if (!on_complete_ || !final_status_) {
    return;
  }

  if (*final_status_ != net::Error::OK) {
    OnError();
    return;
  }
  if (data_complete_) {
    std::move(on_complete_).Run(/*success=*/true);
  }
}

void BlobWriter::OnError() {
  blob_chunk_.reset();
  // This makes sure `on_complete_` isn't run synchronously during `Start()` or
  // `OnDataAvailable()`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_complete_), /*success=*/false));
}

}  // namespace content::indexed_db::sqlite
