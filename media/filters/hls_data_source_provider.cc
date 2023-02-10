// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"

namespace media {

HlsDataSource::~HlsDataSource() = default;

HlsDataSourceProvider::~HlsDataSourceProvider() = default;

HlsDataSourceStream::HlsDataSourceStream(HlsDataSourceStream&&) = default;
HlsDataSourceStream::~HlsDataSourceStream() = default;
HlsDataSourceStream::HlsDataSourceStream(
    std::unique_ptr<HlsDataSource> data_source)
    : data_source_(std::move(data_source)) {}

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

void HlsDataSourceStream::ReadAll(ReadCb read_cb) && {
  std::move(*this).ReadChunk(base::BindOnce(
      [](ReadCb cb, ReadResult m_stream) {
        if (!m_stream.has_value()) {
          std::move(cb).Run(std::move(m_stream).error().AddHere());
          return;
        }
        auto stream = std::move(m_stream).value();
        if (stream.data_source_->GetSize().has_value() &&
            stream.CanReadMore()) {
          std::move(stream).ReadAll(std::move(cb));
          return;
        }
        std::move(cb).Run(std::move(stream));
      },
      std::move(read_cb)));
}

void HlsDataSourceStream::ReadChunk(ReadCb cb, size_t read_size) && {
  size_t original_buffer_size = BytesInBuffer();
  buffer_.insert(buffer_.end(), read_size, 0);
  uint8_t* destination = buffer_.data() + original_buffer_size;

  data_source_->Read(
      total_bytes_read_, read_size, destination,
      base::BindOnce(
          [](ReadCb cb, size_t original_size,
             HlsDataSourceStream captured_stream,
             HlsDataSource::ReadStatus::Or<size_t> result) {
            if (!result.has_value()) {
              std::move(cb).Run(std::move(result).error());
              return;
            }
            size_t bytes_read = std::move(result).value();
            captured_stream.buffer_.resize(original_size + bytes_read);
            captured_stream.total_bytes_read_ += bytes_read;
            std::move(cb).Run(std::move(captured_stream));
          },
          std::move(cb), original_buffer_size, std::move(*this)));
}

}  // namespace media
