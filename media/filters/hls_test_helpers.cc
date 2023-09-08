// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_test_helpers.h"

#include "base/files/file_util.h"
#include "media/base/test_data_util.h"
#include "media/filters/hls_data_source_provider.h"

namespace media {
using testing::_;

namespace {

std::vector<uint8_t> FileToDataVec(const std::string& filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  int64_t file_size = 0;
  CHECK(base::GetFileSize(file_path, &file_size))
      << "Failed to get file size for '" << filename << "'";
  std::vector<uint8_t> result(file_size);
  char* raw = reinterpret_cast<char*>(result.data());
  CHECK_EQ(file_size, base::ReadFile(file_path, raw, file_size))
      << "Failed to read '" << filename << "'";
  return result;
}

std::vector<uint8_t> ContentToDataVec(base::StringPiece content) {
  std::vector<uint8_t> result(content.begin(), content.end());
  return result;
}

}  // namespace

MockManifestDemuxerEngineHost::MockManifestDemuxerEngineHost() = default;
MockManifestDemuxerEngineHost::~MockManifestDemuxerEngineHost() = default;

MockHlsRenditionHost::MockHlsRenditionHost() {}
MockHlsRenditionHost::~MockHlsRenditionHost() {}

void MockHlsRenditionHost::ExchangeStreamId(
    HlsDataSourceStream::StreamId ticket,
    HlsDataSourceStreamManager::ReadCb cb,
    HlsDataSource::ReadStatus::Or<size_t> result) {
  auto it = stream_map_.find(ticket);
  CHECK(it != stream_map_.end());
  auto stream = std::move(it->second);
  stream_map_.erase(it);
  if (result.has_value()) {
    std::move(cb).Run(std::move(stream));
    return;
  }
  std::move(cb).Run(std::move(result).error());
}

void MockHlsRenditionHost::ReadStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceStreamManager::ReadCb cb) {
  auto ticket = stream_ticket_generator_.GenerateNextId();
  stream_map_[ticket] = std::move(stream);
  stream_map_[ticket]->ReadChunkForTesting(
      base::BindOnce(&MockHlsRenditionHost::ExchangeStreamId,
                     base::Unretained(this), ticket, std::move(cb)));
}

MockHlsRendition::MockHlsRendition() = default;
MockHlsRendition::~MockHlsRendition() = default;

FakeHlsDataSource::FakeHlsDataSource(std::vector<uint8_t> data)
    : HlsDataSource(data.size()), data_(std::move(data)) {}

FakeHlsDataSource::~FakeHlsDataSource() {}

void FakeHlsDataSource::Read(uint64_t pos,
                             size_t size,
                             uint8_t* buf,
                             HlsDataSource::ReadCb cb) {
  if (pos > data_.size()) {
    return std::move(cb).Run(HlsDataSource::ReadStatusCodes::kError);
  }
  size_t len = std::min(size, static_cast<size_t>(data_.size() - pos));
  memcpy(buf, &data_[pos], len);
  std::move(cb).Run(len);
}

void FakeHlsDataSource::Stop() {}

base::StringPiece FakeHlsDataSource::GetMimeType() const {
  return "";
}

FileHlsDataSource::FileHlsDataSource(const std::string& filename)
    : FakeHlsDataSource(FileToDataVec(filename)) {}

StringHlsDataSource::StringHlsDataSource(base::StringPiece content)
    : FakeHlsDataSource(ContentToDataVec(content)) {}

}  // namespace media
