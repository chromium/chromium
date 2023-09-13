// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"

namespace media {

namespace {

void DataSourceReadComplete(base::WeakPtr<HlsDataSourceStream> weak_ptr,
                            HlsDataSource::ReadCb cb,
                            size_t original_size,
                            HlsDataSource::ReadStatus::Or<size_t> result) {
  if (!result.has_value()) {
    std::move(cb).Run(std::move(result).error().AddHere());
    return;
  }
  if (!weak_ptr) {
    std::move(cb).Run(HlsDataSource::ReadStatus::Codes::kAborted);
    return;
  }
  auto bytes_read = std::move(result).value();
  weak_ptr->UpdateBytes(original_size, bytes_read);
  std::move(cb).Run(bytes_read);
}

}  // namespace

HlsDataSource::~HlsDataSource() = default;

HlsDataSourceProvider::~HlsDataSourceProvider() = default;
HlsDataSourceStreamManager::~HlsDataSourceStreamManager() = default;

HlsDataSourceStream::~HlsDataSourceStream() {
  data_source_->Stop();
}

HlsDataSourceStream::HlsDataSourceStream(
    std::unique_ptr<HlsDataSource> data_source)
    : data_source_(std::move(data_source)), weak_factory_(this) {}

bool HlsDataSourceStream::CanReadMore() const {
  auto ds_size = data_source_->GetSize();
  if (ds_size.has_value()) {
    return *ds_size > total_bytes_read_;
  }
  // If there's no data size on the source, then assume we can keep reading.
  return true;
}

size_t HlsDataSourceStream::BytesInBuffer() const {
  return buffer_.size();
}

base::StringPiece HlsDataSourceStream::AsStringPiece() const {
  return base::StringPiece(reinterpret_cast<const char*>(buffer_.data()),
                           buffer_.size());
}

const uint8_t* HlsDataSourceStream::AsRawData() const {
  return buffer_.data();
}

void HlsDataSourceStream::Flush() {
  buffer_.resize(0);
}

void HlsDataSourceStream::ReadChunkForTesting(HlsDataSource::ReadCb cb,
                                              size_t size) {
  ReadChunkInternal(std::move(cb), size);
}

void HlsDataSourceStream::ReadChunk(base::PassKey<HlsManifestDemuxerEngine>,
                                    HlsDataSource::ReadCb cb) {
  ReadChunkInternal(std::move(cb), kDefaultReadSize);
}

void HlsDataSourceStream::UpdateBytes(size_t original_size, size_t bytes_read) {
  // TODO(crbug/1266991): Consider swapping out the vector with a more
  // size-flexible data structure to avoid resizing.
  buffer_.resize(original_size + bytes_read);
  total_bytes_read_ += bytes_read;
}

void HlsDataSourceStream::ReadChunkInternal(HlsDataSource::ReadCb cb,
                                            size_t read_size) {
  size_t original_buffer_size = BytesInBuffer();
  buffer_.insert(buffer_.end(), read_size, 0);
  uint8_t* destination = buffer_.data() + original_buffer_size;

  data_source_->Read(
      total_bytes_read_, read_size, destination,
      base::BindOnce(&DataSourceReadComplete, weak_factory_.GetWeakPtr(),
                     std::move(cb), original_buffer_size));
}

}  // namespace media
