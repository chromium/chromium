// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_test_helpers.h"

#include <optional>

#include "base/compiler_specific.h"
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

MockHlsRendition::MockHlsRendition(GURL uri) : uri_(std::move(uri)) {}
MockHlsRendition::~MockHlsRendition() = default;

MockHlsNetworkAccess::MockHlsNetworkAccess() = default;
MockHlsNetworkAccess::~MockHlsNetworkAccess() = default;

void MockHlsRendition::UpdatePlaylistURI(const GURL& uri) {
  MockUpdatePlaylistURI(uri);
  uri_ = uri;
}

const GURL& MockHlsRendition::MediaPlaylistUri() const {
  return uri_;
}

// static
std::unique_ptr<HlsDataSourceStream>
StringHlsDataSourceStreamFactory::CreateStream(std::string content,
                                               bool taint_origin) {
  HlsDataSourceProvider::SegmentQueue segments;
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segments),
      base::DoNothing());
  base::span<uint8_t> buffer = stream->LockStreamForWriting(content.length());
  buffer.copy_from(base::as_byte_span(content));
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
  std::optional<int64_t> file_size = base::GetFileSize(file_path);
  CHECK(file_size.has_value())
      << "Failed to get file size for '" << filename << "'";
  HlsDataSourceProvider::SegmentQueue segments;
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segments),
      base::DoNothing());
  base::span<uint8_t> buffer = stream->LockStreamForWriting(
      base::checked_cast<size_t>(file_size.value()));
  CHECK_EQ(buffer.size(), base::ReadFile(file_path, buffer).value_or(0));
  stream->UnlockStreamPostWrite(base::checked_cast<size_t>(file_size.value()),
                                true);
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
      EXPECT_CALL(*next_mock_,
                  Read(std::get<0>(e), SpanSizeEq(std::get<1>(e)), _))
          .WillOnce(base::test::RunOnceCallback<2>(std::get<2>(e)));
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
