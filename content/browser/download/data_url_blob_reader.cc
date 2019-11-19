// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/data_url_blob_reader.h"

#include "base/strings/string_util.h"

namespace content {

// static
void DataURLBlobReader::ReadDataURLFromBlob(
    mojo::PendingRemote<blink::mojom::Blob> data_url_blob,
    DataURLBlobReader::ReadCompletionCallback read_completion_callback) {
  DataURLBlobReader* reader = new DataURLBlobReader(std::move(data_url_blob));
  auto data_url_blob_reader = base::WrapUnique(reader);

  // Move the reader to be owned by the callback.
  base::OnceClosure closure = base::BindOnce(
      [](ReadCompletionCallback callback,
         std::unique_ptr<DataURLBlobReader> blob_reader) {
        GURL url = base::StartsWith(blob_reader->url_data_,
                                    "data:", base::CompareCase::SENSITIVE)
                       ? GURL(blob_reader->url_data_)
                       : GURL();
        std::move(callback).Run(std::move(url));
      },
      std::move(read_completion_callback), std::move(data_url_blob_reader));
  reader->Start(std::move(closure));
}

DataURLBlobReader::DataURLBlobReader(
    mojo::PendingRemote<blink::mojom::Blob> data_url_blob)
    : data_url_blob_(std::move(data_url_blob)) {
  data_url_blob_.set_disconnect_handler(
      base::BindOnce(&DataURLBlobReader::OnFailed, base::Unretained(this)));
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DataURLBlobReader::~DataURLBlobReader() = default;

void DataURLBlobReader::Start(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run();
    return;
  }

  callback_ = std::move(callback);

  data_url_blob_->ReadAll(std::move(producer_handle), mojo::NullRemote());
  data_pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void DataURLBlobReader::OnDataAvailable(const void* data, size_t num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_data_.append(static_cast<const char*>(data), num_bytes);
}

void DataURLBlobReader::OnDataComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run();
}

void DataURLBlobReader::OnFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_data_.clear();
  std::move(callback_).Run();
}

}  // namespace content
