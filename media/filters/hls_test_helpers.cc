// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_test_helpers.h"

#include "base/files/file_util.h"
#include "media/base/test_data_util.h"
#include "media/filters/hls_data_source_provider.h"

namespace media {
using testing::_;

MockCodecDetector::MockCodecDetector() = default;
MockCodecDetector::~MockCodecDetector() = default;

MockHlsDataSourceProvider::MockHlsDataSourceProvider() = default;
MockHlsDataSourceProvider::~MockHlsDataSourceProvider() = default;

// static
std::unique_ptr<HlsDataSourceStream>
StringHlsDataSourceStreamFactory::CreateStream(std::string content) {
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), base::DoNothing(),
      absl::nullopt);
  auto* buffer = stream->LockStreamForWriting(content.length());
  memcpy(buffer, content.c_str(), content.length());
  stream->UnlockStreamPostWrite(content.length(), true);
  return stream;
}

// static
std::unique_ptr<HlsDataSourceStream>
FileHlsDataSourceStreamFactory::CreateStream(std::string filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  int64_t file_size = 0;
  CHECK(base::GetFileSize(file_path, &file_size))
      << "Failed to get file size for '" << filename << "'";
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), base::DoNothing(),
      absl::nullopt);
  auto* buffer = stream->LockStreamForWriting(file_size);
  CHECK_EQ(file_size, base::ReadFile(file_path, reinterpret_cast<char*>(buffer),
                                     file_size));
  stream->UnlockStreamPostWrite(file_size, true);
  return stream;
}

MockManifestDemuxerEngineHost::MockManifestDemuxerEngineHost() = default;
MockManifestDemuxerEngineHost::~MockManifestDemuxerEngineHost() = default;

MockHlsRenditionHost::MockHlsRenditionHost() {}
MockHlsRenditionHost::~MockHlsRenditionHost() {}

MockHlsRendition::MockHlsRendition() = default;
MockHlsRendition::~MockHlsRendition() = default;

}  // namespace media
