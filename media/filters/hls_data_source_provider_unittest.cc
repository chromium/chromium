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

 private:
  absl::optional<size_t> remaining_ = absl::nullopt;
  std::string pattern_;
};

HlsDataSourceStream GetUnlimitedStream() {
  return HlsDataSourceStream(std::make_unique<FakeHlsDataSource>(
      absl::nullopt, "The Quick Brown Fox Jumped Over The Lazy Dog"));
}

HlsDataSourceStream GetLimitedStream() {
  return HlsDataSourceStream(std::make_unique<FakeHlsDataSource>(
      44, "The Quick Brown Fox Jumped Over The Lazy Dog"));
}

HlsDataSourceStream GetMassiveStream(bool limited) {
  std::stringstream ss;
  for (int i = 0; i < 1000; i++) {
    ss << "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
       << "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
       << "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
       << "aliquip ex ea commodo consequat. Duis aute irure dolor in "
       << "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
       << "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
       << "culpa qui officia deserunt mollit anim id est laborum.\n";
  }
  std::string content = ss.str();
  absl::optional<uint64_t> size = absl::nullopt;
  if (limited) {
    size = content.size();
  }
  return HlsDataSourceStream(
      std::make_unique<FakeHlsDataSource>(size, content));
}

}  // namespace

TEST(HlsDataSourceStreamUnittest, TestReadAllFromLimitedStream) {
  GetLimitedStream().ReadAll(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_FALSE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)44);
        ASSERT_EQ(std::string(stream.AsStringPiece()),
                  "The Quick Brown Fox Jumped Over The Lazy Dog");
      }));
}

TEST(HlsDataSourceStreamUnittest, TestReadAllFromUnlimitedStream) {
  GetUnlimitedStream().ReadAll(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)0xFFFF);
        // make sure it's repeating.
        for (int i = 0; i < 4; i++) {
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 0], 'T');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 1], 'h');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 2], 'e');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 4], 'Q');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 5], 'u');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 6], 'i');
        }
      }));
}

TEST(HlsDataSourceStreamUnittest, TestReadDefaultChunkFromLimitedStream) {
  // Read the default size chunk (0xFFFF). Should be pretty much the same
  // as reading all from a limited stream.
  GetLimitedStream().ReadChunk(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_FALSE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)44);
        ASSERT_EQ(std::string(stream.AsStringPiece()),
                  "The Quick Brown Fox Jumped Over The Lazy Dog");
      }));
}

TEST(HlsDataSourceStreamUnittest, TestReadDefaultChunkFromUnlimitedStream) {
  // Read the default size chunk (0xFFFF). Should be pretty much the same
  // as reading all from an unlimited stream, which just reverts to chunks.
  GetUnlimitedStream().ReadChunk(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)0xFFFF);
        // make sure it's repeating.
        for (int i = 0; i < 4; i++) {
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 0], 'T');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 1], 'h');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 2], 'e');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 4], 'Q');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 5], 'u');
          ASSERT_EQ(stream.AsStringPiece()[i * 44 + 6], 'i');
        }
      }));
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeFromLimitedStream) {
  GetLimitedStream().ReadChunk(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)14);
        ASSERT_EQ(std::string(stream.AsStringPiece()), "The Quick Brow");

        // Read it again!
        std::move(stream).ReadChunk(
            base::BindOnce([](HlsDataSourceStream::ReadResult result) {
              ASSERT_TRUE(result.has_value());
              auto stream = std::move(result).value();
              ASSERT_TRUE(stream.CanReadMore());
              ASSERT_EQ(stream.BytesInBuffer(), (size_t)28);
              ASSERT_EQ(std::string(stream.AsStringPiece()),
                        "The Quick Brown Fox Jumped O");
            }),
            14);
      }),
      14);
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeFromUnlimitedStream) {
  GetUnlimitedStream().ReadChunk(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)14);
        ASSERT_EQ(std::string(stream.AsStringPiece()), "The Quick Brow");

        // Read it again!
        std::move(stream).ReadChunk(
            base::BindOnce([](HlsDataSourceStream::ReadResult result) {
              ASSERT_TRUE(result.has_value());
              auto stream = std::move(result).value();
              ASSERT_TRUE(stream.CanReadMore());
              ASSERT_EQ(stream.BytesInBuffer(), (size_t)28);
              ASSERT_EQ(std::string(stream.AsStringPiece()),
                        "The Quick Brown Fox Jumped O");
            }),
            14);
      }),
      14);
}

TEST(HlsDataSourceStreamUnittest, TestReadSmallSizeWithFlush) {
  GetUnlimitedStream().ReadChunk(
      base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)14);
        ASSERT_EQ(std::string(stream.AsStringPiece()), "The Quick Brow");

        // clear the buffer
        stream.Flush();

        // Read it again!
        std::move(stream).ReadChunk(
            base::BindOnce([](HlsDataSourceStream::ReadResult result) {
              ASSERT_TRUE(result.has_value());
              auto stream = std::move(result).value();
              ASSERT_TRUE(stream.CanReadMore());
              ASSERT_EQ(stream.BytesInBuffer(), (size_t)14);
              ASSERT_EQ(std::string(stream.AsStringPiece()), "n Fox Jumped O");
            }),
            14);
      }),
      14);
}

TEST(HlsDataSourceStreamUnittest, ReadAllFromMultiChunkStream) {
  // Read from a stream longer than 0xFFFF bytes, to assert that |ReadAll|
  // really does read all the data from a size-limited stream.
  GetMassiveStream(/*limited=*/true)
      .ReadAll(base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_FALSE(stream.CanReadMore());
        // 0xFFFF   =  65535
        // readsize = 446000
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)446000);
      }));
}

TEST(HlsDataSourceStreamUnittest, ReadAllFromMultiChunkStreamUnknownLength) {
  // Read from a stream longer than 0xFFFF bytes, to assert that |ReadAll|
  // really does read all the data from a size-limited stream.
  GetMassiveStream(/*limited=*/false)
      .ReadAll(base::BindOnce([](HlsDataSourceStream::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream.CanReadMore());
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)0xFFFF);

        // clear the buffer
        stream.Flush();
        ASSERT_EQ(stream.BytesInBuffer(), (size_t)0);

        std::move(stream).ReadAll(
            base::BindOnce([](HlsDataSourceStream::ReadResult res) {
              ASSERT_TRUE(res.has_value());
              auto stream = std::move(res).value();
              // 0xFFFF * 2 is still less than stream size.
              ASSERT_TRUE(stream.CanReadMore());
              ASSERT_EQ(stream.BytesInBuffer(), (size_t)0xFFFF);
            }));
      }));
}

}  // namespace media::hls
