// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/blob_writer.h"

#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace content::indexed_db::sqlite {

// static
std::unique_ptr<BlobWriter> BlobWriter::WriteBlobIntoDatabase(
    IndexedDBExternalObject& external_object,
    sql::StreamingBlobHandle blob_handle,
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
      new BlobWriter(std::move(blob_handle), std::move(on_complete)));
  sink->Start(std::move(consumer_handle));
  return sink;
}

BlobWriter::BlobWriter(sql::StreamingBlobHandle blob_handle,
                       base::OnceCallback<void(bool)> on_complete)
    : target_(std::move(blob_handle)), on_complete_(std::move(on_complete)) {}

BlobWriter::~BlobWriter() = default;

void BlobWriter::Start(mojo::ScopedDataPipeConsumerHandle consumer_handle) {
  drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void BlobWriter::OnDataAvailable(base::span<const uint8_t> data) {
  if (!target_) {
    return;
  }
  if (target_->Write(bytes_written_so_far_, data)) {
    bytes_written_so_far_ += data.size();
  } else {
    target_.reset();
    // Reporting an error deletes `this`, but `drainer_` doesn't like being
    // deleted inside `OnDataAvailable`.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_complete_), /*success=*/false));
  }
}

void BlobWriter::OnDataComplete() {
  if (target_ && on_complete_) {
    std::move(on_complete_).Run(/*success=*/true);
  }
}

}  // namespace content::indexed_db::sqlite
