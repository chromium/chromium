// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_reader.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_view_util.h"
#include "net/base/net_errors.h"

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback,
                      uint64_t offset,
                      uint64_t length) {
  CHECK_GE(offset, 0U);
  CHECK_GT(length, 0U);
  CHECK_LE(offset, std::numeric_limits<uint64_t>::max() - length);

  Read(std::move(blob), std::move(callback), Range{offset, length});
}

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback) {
  Read(std::move(blob), std::move(callback), /*range=*/std::nullopt);
}

BlobReader::~BlobReader() {
  std::move(callback_).Run(std::move(blob_data_), blob_length_.value_or(0));
}

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback,
                      std::optional<BlobReader::Range> range) {
  (new BlobReader(std::move(blob), std::move(callback), std::move(range)))
      ->Start();
}

BlobReader::BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob,
                       BlobReadCallback callback,
                       std::optional<Range> range)
    : callback_(std::move(callback)),
      blob_(std::move(blob)),
      read_range_(std::move(range)) {}

void BlobReader::Start() {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result = CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    delete this;
    return;
  }
  if (read_range_) {
    blob_->ReadRange(read_range_->offset, read_range_->length,
                     std::move(producer_handle),
                     receiver_.BindNewPipeAndPassRemote());
  } else {
    blob_->ReadAll(std::move(producer_handle),
                   receiver_.BindNewPipeAndPassRemote());
  }
  receiver_.set_disconnect_handler(
      base::BindOnce(&BlobReader::OnDisconnected, base::Unretained(this)));
  data_pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void BlobReader::OnCalculatedSize(uint64_t total_size,
                                  uint64_t expected_content_size) {
  blob_length_ = total_size;
}

void BlobReader::OnComplete(int32_t status, uint64_t data_length) {
  status_ = status;
  MaybeComplete();
}

void BlobReader::OnDataAvailable(base::span<const uint8_t> data) {
  blob_data_.append(base::as_string_view(data));
}

void BlobReader::OnDataComplete() {
  data_complete_ = true;
  MaybeComplete();
}

void BlobReader::OnDisconnected() {
  if (!status_) {
    status_ = net::ERR_FAILED;
  }
  MaybeComplete();
}

void BlobReader::MaybeComplete() {
  if (!status_) {
    return;
  }

  if (status_ != net::OK) {
    blob_length_ = 0;
    delete this;
  } else if (data_complete_) {
    delete this;
  }
}
