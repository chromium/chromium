// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class HlsDataSourceStreamUnittest : public testing::Test {
 public:
  HlsDataSourceStreamUnittest() = default;

  void OnReleaseInternal(HlsDataSourceStream::StreamId id) { OnRelease(id); }

  MOCK_METHOD(void, OnRelease, (HlsDataSourceStream::StreamId), ());

 protected:
  std::unique_ptr<HlsDataSourceStream> CreateStream(
      std::optional<hls::types::ByteRange> range) {
    HlsDataSourceProvider::SegmentQueue segments;
    segments.emplace(GURL("https://example.com"), range);
    return CreateStream(std::move(segments));
  }

  std::unique_ptr<HlsDataSourceStream> CreateStream(
      HlsDataSourceProvider::SegmentQueue queue) {
    auto stream_id = stream_id_generator_.GenerateNextId();
    EXPECT_CALL(*this, OnRelease(stream_id));
    return std::make_unique<HlsDataSourceStream>(
        stream_id, std::move(queue),
        base::BindOnce(&HlsDataSourceStreamUnittest::OnReleaseInternal,
                       base::Unretained(this), stream_id));
  }

  HlsDataSourceStream::StreamId::Generator stream_id_generator_;
};

TEST_F(HlsDataSourceStreamUnittest, TestNewStreamHasProperties) {
  auto stream = CreateStream(std::nullopt);

  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_TRUE(stream->CanReadMore());
  ASSERT_TRUE(stream->RequiresNextDataSource());
  ASSERT_EQ(stream->GetNextSegmentURI(), GURL("https://example.com"));
  ASSERT_FALSE(stream->RequiresNextDataSource());

  auto capped = CreateStream(hls::types::ByteRange::Validate(10, 20));
  ASSERT_EQ(capped->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(capped->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(capped->max_read_position(), std::nullopt);
  ASSERT_TRUE(capped->CanReadMore());
  ASSERT_TRUE(capped->RequiresNextDataSource());
  ASSERT_EQ(capped->GetNextSegmentURI(), GURL("https://example.com"));
  ASSERT_FALSE(capped->RequiresNextDataSource());

  HlsDataSourceProvider::SegmentQueue segments;
  segments.emplace(GURL("https://example.com"), std::nullopt);
  segments.emplace(GURL("https://foo.com"), std::nullopt);
  auto double_url = CreateStream(std::move(segments));
  ASSERT_EQ(double_url->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(double_url->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(double_url->max_read_position(), std::nullopt);
  ASSERT_TRUE(double_url->CanReadMore());
  ASSERT_TRUE(double_url->RequiresNextDataSource());
  ASSERT_EQ(double_url->GetNextSegmentURI(), GURL("https://example.com"));
  ASSERT_FALSE(double_url->RequiresNextDataSource());
  double_url->LockStreamForWriting(4);
  double_url->UnlockStreamPostWrite(0, true);
  ASSERT_TRUE(double_url->RequiresNextDataSource());
  ASSERT_EQ(double_url->GetNextSegmentURI(), GURL("https://foo.com"));
  ASSERT_FALSE(double_url->RequiresNextDataSource());
}

TEST_F(HlsDataSourceStreamUnittest, TestWritesAndClears) {
  auto stream = CreateStream(std::nullopt);

  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  // No changes, because no write.
  stream->UnlockStreamPostWrite(0, false);
  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  stream->UnlockStreamPostWrite(10, false);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(10));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);
  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(10));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(20));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  stream->UnlockStreamPostWrite(10, false);
  stream->Clear();
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(20));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(stream->max_read_position(), std::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);
}

}  // namespace media
