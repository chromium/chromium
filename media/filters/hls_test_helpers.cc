// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_test_helpers.h"

#include "base/files/file_util.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/test_data_util.h"
#include "media/filters/hls_data_source_provider.h"

namespace media {
using testing::_;

MockDataSource::~MockDataSource() = default;
MockDataSource::MockDataSource() = default;

MockHlsDataSourceProvider::MockHlsDataSourceProvider() = default;
MockHlsDataSourceProvider::~MockHlsDataSourceProvider() = default;

MockManifestDemuxerEngineHost::MockManifestDemuxerEngineHost() = default;
MockManifestDemuxerEngineHost::~MockManifestDemuxerEngineHost() = default;

MockHlsRenditionHost::MockHlsRenditionHost() = default;
MockHlsRenditionHost::~MockHlsRenditionHost() = default;

MockHlsRendition::MockHlsRendition() = default;
MockHlsRendition::~MockHlsRendition() = default;

MockHlsNetworkAccess::MockHlsNetworkAccess() = default;
MockHlsNetworkAccess::~MockHlsNetworkAccess() = default;

// static
std::unique_ptr<HlsDataSourceStream>
StringHlsDataSourceStreamFactory::CreateStream(std::string content,
                                               bool taint_origin) {
  HlsDataSourceProvider::SegmentQueue segments;
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segments),
      base::DoNothing());
  auto* buffer = stream->LockStreamForWriting(content.length());
  memcpy(buffer, content.c_str(), content.length());
  stream->UnlockStreamPostWrite(content.length(), true);
  if (taint_origin) {
    stream->set_would_taint_origin();
  }
  return stream;
}

// static
std::unique_ptr<HlsDataSourceStream>
FileHlsDataSourceStreamFactory::CreateStream(std::string filename,
                                             bool taint_origin) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  int64_t file_size = 0;
  CHECK(base::GetFileSize(file_path, &file_size))
      << "Failed to get file size for '" << filename << "'";
  HlsDataSourceProvider::SegmentQueue segments;
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segments),
      base::DoNothing());
  auto* buffer = stream->LockStreamForWriting(file_size);
  CHECK_EQ(file_size, base::ReadFile(file_path, reinterpret_cast<char*>(buffer),
                                     file_size));
  stream->UnlockStreamPostWrite(file_size, true);
  if (taint_origin) {
    stream->set_would_taint_origin();
  }
  return stream;
}

MockDataSourceFactory::~MockDataSourceFactory() = default;
MockDataSourceFactory::MockDataSourceFactory() = default;

void MockDataSourceFactory::CreateDataSource(GURL uri, bool, DataSourceCb cb) {
  if (!next_mock_) {
    PregenerateNextMock();
    EXPECT_CALL(*next_mock_, Initialize)
        .WillOnce(base::test::RunOnceCallback<0>(true));
    for (const auto& e : read_expectations_) {
      EXPECT_CALL(*next_mock_, Read(std::get<0>(e), std::get<1>(e), _, _))
          .WillOnce(base::test::RunOnceCallback<3>(std::get<2>(e)));
    }
    read_expectations_.clear();
    EXPECT_CALL(*next_mock_, Stop());
  }
  std::move(cb).Run(std::move(next_mock_));
}

void MockDataSourceFactory::AddReadExpectation(size_t from,
                                               size_t to,
                                               int response) {
  read_expectations_.emplace_back(from, to, response);
}

testing::NiceMock<MockDataSource>*
MockDataSourceFactory::PregenerateNextMock() {
  next_mock_ = std::make_unique<testing::NiceMock<MockDataSource>>();
  return next_mock_.get();
}

}  // namespace media
