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
      absl::optional<hls::types::ByteRange> range) {
    auto stream_id = stream_id_generator_.GenerateNextId();
    EXPECT_CALL(*this, OnRelease(stream_id));
    return std::make_unique<HlsDataSourceStream>(
        stream_id,
        base::BindOnce(&HlsDataSourceStreamUnittest::OnReleaseInternal,
                       base::Unretained(this), stream_id),
        range);
  }

  HlsDataSourceStream::StreamId::Generator stream_id_generator_;
};

TEST_F(HlsDataSourceStreamUnittest, TestNewStreamHasProperties) {
  auto stream = CreateStream(absl::nullopt);

  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  auto capped = CreateStream(hls::types::ByteRange::Validate(10, 20));
  ASSERT_EQ(capped->read_position(), static_cast<size_t>(20));
  ASSERT_EQ(capped->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(capped->max_read_position().value_or(0), static_cast<size_t>(30));
  ASSERT_EQ(capped->CanReadMore(), true);
}

TEST_F(HlsDataSourceStreamUnittest, TestWritesAndClears) {
  auto stream = CreateStream(absl::nullopt);

  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  // No changes, because no write.
  stream->UnlockStreamPostWrite(0, false);
  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(0));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  stream->UnlockStreamPostWrite(10, false);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(10));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(10));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  stream->LockStreamForWriting(10);
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(10));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(20));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);

  stream->UnlockStreamPostWrite(10, false);
  stream->Clear();
  ASSERT_EQ(stream->read_position(), static_cast<size_t>(20));
  ASSERT_EQ(stream->buffer_size(), static_cast<size_t>(0));
  ASSERT_EQ(stream->max_read_position(), absl::nullopt);
  ASSERT_EQ(stream->CanReadMore(), true);
}

}  // namespace media
