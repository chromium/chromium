// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_test_helpers.h"

#include <algorithm>
#include <optional>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
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
StringHlsDataSourceStreamFactory::CreateStream(
    std::string content,
    std::optional<hls::SecurityMetadata> info,
    GURL uri) {
  HlsDataSourceProvider::UrlDataSegment segment(uri, std::nullopt);
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segment),
      base::DoNothing());
  base::span<uint8_t> buffer = stream->LockStreamForWriting(content.length());
  buffer.copy_from(base::as_byte_span(content));
  stream->UnlockStreamPostWrite(content.length(), true);
  stream->SetPostRedirectUri(uri);
  if (info.has_value()) {
    stream->SetSecurityInfoForTesting(*info);
  }
  return stream;
}

// static
std::unique_ptr<HlsDataSourceStream>
FileHlsDataSourceStreamFactory::CreateStream(
    std::string filename,
    std::optional<hls::SecurityMetadata> info,
    GURL uri) {
  base::FilePath file_path = GetTestDataFilePath(filename);
  std::optional<int64_t> file_size = base::GetFileSize(file_path);
  CHECK(file_size.has_value())
      << "Failed to get file size for '" << filename << "'";
  HlsDataSourceProvider::UrlDataSegment segment(uri, std::nullopt);
  auto stream = std::make_unique<HlsDataSourceStream>(
      HlsDataSourceStream::StreamId::FromUnsafeValue(42), std::move(segment),
      base::DoNothing());
  base::span<uint8_t> buffer = stream->LockStreamForWriting(
      base::checked_cast<size_t>(file_size.value()));
  CHECK_EQ(buffer.size(), base::ReadFile(file_path, buffer).value_or(0));
  stream->UnlockStreamPostWrite(base::checked_cast<size_t>(file_size.value()),
                                true);
  stream->SetPostRedirectUri(uri);
  if (info.has_value()) {
    stream->SetSecurityInfoForTesting(*info);
  }
  return stream;
}

MockDataSourceFactory::DataSourceBehavior::DataSourceBehavior() = default;
MockDataSourceFactory::DataSourceBehavior::~DataSourceBehavior() = default;
MockDataSourceFactory::DataSourceBehavior::DataSourceBehavior(
    const DataSourceBehavior&) = default;
MockDataSourceFactory::DataSourceBehavior::DataSourceBehavior(
    DataSourceBehavior&&) = default;
MockDataSourceFactory::DataSourceBehavior&
MockDataSourceFactory::DataSourceBehavior::operator=(
    const DataSourceBehavior&) = default;
MockDataSourceFactory::DataSourceBehavior&
MockDataSourceFactory::DataSourceBehavior::operator=(DataSourceBehavior&&) =
    default;

MockDataSourceFactory::~MockDataSourceFactory() = default;

MockDataSourceFactory::MockDataSourceFactory() {
  ON_CALL(*this, Setup(_, _, _, _))
      .WillByDefault(
          testing::Invoke(this, &MockDataSourceFactory::DefaultSetup));
}

void MockDataSourceFactory::Create(
    const GURL& uri,
    DataSource::CacheMode cache_mode,
    DataSource::EncodingMode encoding_mode,
    base::OnceCallback<void(std::unique_ptr<CrossOriginDataSource>)> cb) {
  auto mock = std::make_unique<testing::NiceMock<MockDataSource>>();
  Setup(mock.get(), uri, cache_mode, encoding_mode);
  std::move(cb).Run(std::move(mock));
}

void MockDataSourceFactory::Expect(DataSourceBehavior behavior) {
  expected_behaviors_.push_back(std::move(behavior));
}

void MockDataSourceFactory::AddReadExpectation(size_t from,
                                               size_t to,
                                               int response) {
  global_read_expectations_.emplace_back(from, to, response);
}

void MockDataSourceFactory::AddReadExpectation(const GURL& url,
                                               size_t from,
                                               size_t to,
                                               int response) {
  url_read_expectations_.emplace_back(url, std::make_tuple(from, to, response));
}

// static
void MockDataSourceFactory::ConfigureAsSuccess(MockDataSource* mock,
                                               const GURL& uri) {
  ON_CALL(*mock, Initialize(_))
      .WillByDefault(base::test::RunOnceCallback<0>(true));
  ON_CALL(*mock, Stop()).WillByDefault(testing::Return());
  ON_CALL(*mock, GetUrlAfterRedirects()).WillByDefault(testing::Return(uri));
  ON_CALL(*mock, GetUrlDataOrigin()).WillByDefault(testing::Return(uri));
  ON_CALL(*mock, DidRedirect()).WillByDefault(testing::Return(false));
  ON_CALL(*mock, WouldTaintOrigin()).WillByDefault(testing::Return(false));
}

// static
void MockDataSourceFactory::ConfigureAsFailure(MockDataSource* mock) {
  ON_CALL(*mock, Initialize(_))
      .WillByDefault(base::test::RunOnceCallback<0>(false));
  EXPECT_CALL(*mock, Stop()).Times(0);
  EXPECT_CALL(*mock, GetUrlAfterRedirects()).Times(0);
  EXPECT_CALL(*mock, GetUrlDataOrigin()).Times(0);
  EXPECT_CALL(*mock, DidRedirect()).Times(0);
  ON_CALL(*mock, WouldTaintOrigin()).WillByDefault(testing::Return(false));
}

// static
void MockDataSourceFactory::ConfigureAsRedirect(MockDataSource* mock,
                                                const GURL& target_uri) {
  ON_CALL(*mock, Initialize(_))
      .WillByDefault(base::test::RunOnceCallback<0>(true));
  ON_CALL(*mock, Stop()).WillByDefault(testing::Return());
  ON_CALL(*mock, GetUrlAfterRedirects())
      .WillByDefault(testing::Return(target_uri));
  ON_CALL(*mock, GetUrlDataOrigin()).WillByDefault(testing::Return(target_uri));
  ON_CALL(*mock, DidRedirect()).WillByDefault(testing::Return(true));
  ON_CALL(*mock, WouldTaintOrigin()).WillByDefault(testing::Return(false));
}

void MockDataSourceFactory::DefaultSetup(
    MockDataSource* mock,
    const GURL& uri,
    DataSource::CacheMode cache_mode,
    DataSource::EncodingMode encoding_mode) {
  // 1. Check if there is a registered behavior for this URL.
  auto behavior_it =
      std::find_if(expected_behaviors_.begin(), expected_behaviors_.end(),
                   [&uri](const auto& b) { return b.original_uri == uri; });
  if (behavior_it != expected_behaviors_.end()) {
    const auto& behavior = *behavior_it;
    if (!behavior.does_connect) {
      ConfigureAsFailure(mock);
    } else if (behavior.redirect_uri) {
      ConfigureAsRedirect(mock, GURL(*behavior.redirect_uri));
    } else {
      ConfigureAsSuccess(mock, uri);
    }

    ON_CALL(*mock, WouldTaintOrigin())
        .WillByDefault(testing::Return(behavior.would_taint_origin));

    if (behavior.does_connect) {
      for (const auto& [from, to, size] : behavior.read_expectations) {
        EXPECT_CALL(*mock, Read(from, SpanSizeEq(to), _))
            .WillOnce(base::test::RunOnceCallback<2>(size));
      }
    }
    expected_behaviors_.erase(behavior_it);
    return;
  }

  // 2. Otherwise, use default success and apply simple read expectations.
  ConfigureAsSuccess(mock, uri);

  // Apply URL-specific expectations first.
  for (auto it = url_read_expectations_.begin();
       it != url_read_expectations_.end();) {
    if (it->first == uri) {
      auto [from, to, size] = it->second;
      EXPECT_CALL(*mock, Read(from, SpanSizeEq(to), _))
          .WillOnce(base::test::RunOnceCallback<2>(size));
      it = url_read_expectations_.erase(it);
    } else {
      ++it;
    }
  }

  // Apply global expectations.
  for (const auto& [from, to, size] : global_read_expectations_) {
    EXPECT_CALL(*mock, Read(from, SpanSizeEq(to), _))
        .WillOnce(base::test::RunOnceCallback<2>(size));
  }
  global_read_expectations_.clear();
}

}  // namespace media
