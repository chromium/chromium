// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_reader.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback,
                      uint64_t offset,
                      uint64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GE(offset, 0U);
  CHECK_GT(length, 0U);
  CHECK_LE(offset, std::numeric_limits<uint64_t>::max() - length);

  Read(std::move(blob), std::move(callback), Range{offset, length});
}

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Read(std::move(blob), std::move(callback), /*range=*/std::nullopt);
}

BlobReader::~BlobReader() { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }

// static
void BlobReader::Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                      BlobReadCallback callback,
                      std::optional<BlobReader::Range> range) {
  auto reader =
      base::WrapUnique(new BlobReader(std::move(blob), std::move(range)));

  // Move the reader to be owned by the callback, so hold onto a temporary
  // pointer to it so we can still call Start on it.
  BlobReader* raw_reader = reader.get();
  base::OnceClosure wrapped = base::BindOnce(
      [](BlobReadCallback callback, std::unique_ptr<BlobReader> reader) {
        std::move(callback).Run(std::move(reader->blob_data_),
                                *reader->blob_length_);
      },
      std::move(callback), std::move(reader));
  raw_reader->Start(std::move(wrapped));
}

BlobReader::BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob,
                       std::optional<Range> range)
    : blob_(std::move(blob)), read_range_(std::move(range)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  blob_.set_disconnect_handler(
      base::BindOnce(&BlobReader::Failed, base::Unretained(this)));
}

void BlobReader::Start(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_ = std::move(callback);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result = CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    Failed();
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
  data_pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void BlobReader::OnCalculatedSize(uint64_t total_size,
                                  uint64_t expected_content_size) {
  blob_length_ = total_size;
  if (data_complete_) {
    Succeeded();
  }
}

void BlobReader::OnDataAvailable(base::span<const uint8_t> data) {
  blob_data_.append(base::as_string_view(data));
}

void BlobReader::OnDataComplete() {
  data_complete_ = true;
  if (blob_length_) {
    Succeeded();
  }
}

void BlobReader::Failed() {
  blob_length_ = 0;
  std::move(callback_).Run();
}

void BlobReader::Succeeded() {
  std::move(callback_).Run();
}
