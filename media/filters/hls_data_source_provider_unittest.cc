// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

namespace {

class FakeHlsDataSource : public HlsDataSource {
 public:
  FakeHlsDataSource(absl::optional<uint64_t> size, std::string pattern)
      : HlsDataSource(size), pattern_(pattern) {
    remaining_ = size;
  }
  ~FakeHlsDataSource() override {}
  void Read(uint64_t pos,
            size_t size,
            uint8_t* buf,
            HlsDataSource::ReadCb cb) override {
    size_t offset = pos % pattern_.length();
    size_t bytes = std::min(remaining_.value_or(size), size);
    if (!bytes) {
      std::move(cb).Run(HlsDataSource::ReadStatusCodes::kError);
    }
    for (size_t i = 0; i < bytes; i++) {
      buf[i] = pattern_[(offset + i) % pattern_.length()];
    }
    if (remaining_.has_value()) {
      *remaining_ -= bytes;
    }
    std::move(cb).Run(bytes);
  }
  base::StringPiece GetMimeType() const override { return "INVALID"; }
  void Stop() override {}

 private:
  absl::optional<size_t> remaining_ = absl::nullopt;
  std::string pattern_;
};

std::unique_ptr<HlsDataSourceStream> GetUnlimitedStream() {
  return std::make_unique<HlsDataSourceStream>(
      std::make_unique<FakeHlsDataSource>(
          absl::nullopt, "The Quick Brown Fox Jumped Over The Lazy Dog"));
}

std::unique_ptr<HlsDataSourceStream> GetLimitedStream() {
  return std::make_unique<HlsDataSourceStream>(
      std::make_unique<FakeHlsDataSource>(
          44, "The Quick Brown Fox Jumped Over The Lazy Dog"));
}

}  // namespace

TEST(HlsDataSourceStreamUnittest, TestReadDefaultChunkFromLimitedStream) {
  // Read the default size chunk (0xFFFF). Should be pretty much the same
  // as reading all from a limited stream.
  auto stream = GetLimitedStream();
  stream->ReadChunkForTesting(base::BindOnce(
      [](HlsDataSourceStream* stream_ptr,
         HlsDataSource::ReadStatus::Or<size_t> result) {
        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(stream_ptr->CanReadMore());
        ASSERT_EQ(stream_ptr->BytesInBuffer(), 44u);
        ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                  "The Quick Brown Fox Jumped Over The Lazy Dog");
      },
      stream.get()));
}

TEST(HlsDataSourceStreamUnittest, TestReadDefaultChunkFromUnlimitedStream) {
  // Read the default size chunk (0xFFFF). Should be pretty much the same
  // as reading all from an unlimited stream, which just reverts to chunks.
  auto stream = GetUnlimitedStream();
  stream->ReadChunkForTesting(base::BindOnce(
      [](HlsDataSourceStream* stream_ptr,
         HlsDataSource::ReadStatus::Or<size_t> result) {
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(stream_ptr->CanReadMore());
        ASSERT_EQ(stream_ptr->BytesInBuffer(), 0x4000u);
        // make sure it's repeating.
        for (int i = 0; i < 4; i++) {
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 0], 'T');
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 1], 'h');
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 2], 'e');
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 4], 'Q');
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 5], 'u');
          ASSERT_EQ(stream_ptr->AsStringPiece()[i * 44 + 6], 'i');
        }
      },
      stream.get()));
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeFromLimitedStream) {
  auto stream = GetLimitedStream();
  stream->ReadChunkForTesting(
      base::BindOnce(
          [](HlsDataSourceStream* stream_ptr,
             HlsDataSource::ReadStatus::Or<size_t> result) {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(stream_ptr->CanReadMore());
            ASSERT_EQ(stream_ptr->BytesInBuffer(), 14u);
            ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                      "The Quick Brow");

            // Read it again!
            stream_ptr->ReadChunkForTesting(
                base::BindOnce(
                    [](HlsDataSourceStream* stream_ptr,
                       HlsDataSource::ReadStatus::Or<size_t> result) {
                      ASSERT_TRUE(result.has_value());
                      ASSERT_TRUE(stream_ptr->CanReadMore());
                      ASSERT_EQ(stream_ptr->BytesInBuffer(), 28u);
                      ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                                "The Quick Brown Fox Jumped O");
                    },
                    stream_ptr),
                14);
          },
          stream.get()),
      14);
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeFromUnlimitedStream) {
  auto stream = GetUnlimitedStream();
  stream->ReadChunkForTesting(
      base::BindOnce(
          [](HlsDataSourceStream* stream_ptr,
             HlsDataSource::ReadStatus::Or<size_t> result) {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(stream_ptr->CanReadMore());
            ASSERT_EQ(stream_ptr->BytesInBuffer(), 14u);
            ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                      "The Quick Brow");

            // Read it again!
            stream_ptr->ReadChunkForTesting(
                base::BindOnce(
                    [](HlsDataSourceStream* stream_ptr,
                       HlsDataSource::ReadStatus::Or<size_t> result) {
                      ASSERT_TRUE(result.has_value());
                      ASSERT_TRUE(stream_ptr->CanReadMore());
                      ASSERT_EQ(stream_ptr->BytesInBuffer(), 28u);
                      ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                                "The Quick Brown Fox Jumped O");
                    },
                    stream_ptr),
                14);
          },
          stream.get()),
      14);
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeWithFlush) {
  auto stream = GetUnlimitedStream();
  stream->ReadChunkForTesting(
      base::BindOnce(
          [](HlsDataSourceStream* stream_ptr,
             HlsDataSource::ReadStatus::Or<size_t> result) {
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(stream_ptr->CanReadMore());
            ASSERT_EQ(stream_ptr->BytesInBuffer(), 14u);
            ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                      "The Quick Brow");

            // clear the buffer
            stream_ptr->Flush();

            // Read it again!
            stream_ptr->ReadChunkForTesting(
                base::BindOnce(
                    [](HlsDataSourceStream* stream_ptr,
                       HlsDataSource::ReadStatus::Or<size_t> result) {
                      ASSERT_TRUE(result.has_value());
                      ASSERT_TRUE(stream_ptr->CanReadMore());
                      ASSERT_EQ(stream_ptr->BytesInBuffer(), 14u);
                      ASSERT_EQ(std::string(stream_ptr->AsStringPiece()),
                                "n Fox Jumped O");
                    },
                    stream_ptr),
                14);
          },
          stream.get()),
      14);
}

}  // namespace media::hls
