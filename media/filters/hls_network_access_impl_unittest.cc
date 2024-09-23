// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_network_access_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_rendition_impl.h"
#include "media/filters/hls_test_helpers.h"
namespace media {

namespace {

enum class KeyMode {
  kPresent,
  kAbsent,
};

enum class InitMode {
  kPresent,
  kAbsent,
};

}  // namespace

using testing::_;

class HlsNetworkAccessImplUnittest : public testing::Test {
 public:
  ~HlsNetworkAccessImplUnittest() override = default;
  HlsNetworkAccessImplUnittest() { RecreateImpl(); }

  void RecreateImpl() {
    auto factory = std::make_unique<MockDataSourceFactory>();
    factory_ = factory.get();
    base::SequenceBound<HlsDataSourceProviderImpl> dsp(
        task_environment_.GetMainThreadTaskRunner(), std::move(factory));
    network_access_ = std::make_unique<HlsNetworkAccessImpl>(std::move(dsp));
  }

  std::optional<hls::types::ByteRange> ByteRangeFromTuple(
      std::optional<std::tuple<uint64_t, uint64_t>> tuple) {
    if (tuple.has_value()) {
      return hls::types::ByteRange::Validate(std::get<0>(*tuple),
                                             std::get<1>(*tuple));
    }
    return std::nullopt;
  }

  scoped_refptr<hls::MediaSegment> MakeSegment(
      std::optional<std::tuple<uint64_t, uint64_t>> byte_range,
      std::optional<std::tuple<uint64_t, uint64_t>> init_br,
      InitMode init_mode = InitMode::kAbsent,
      KeyMode key_mode = KeyMode::kAbsent) {
    scoped_refptr<hls::MediaSegment::InitializationSegment> init = nullptr;
    scoped_refptr<hls::MediaSegment::EncryptionData> enc_data = nullptr;
    if (init_mode == InitMode::kPresent) {
      init = base::MakeRefCounted<hls::MediaSegment::InitializationSegment>(
          GURL("https://foo.com"), ByteRangeFromTuple(init_br));
    }
    if (key_mode == KeyMode::kPresent) {
      enc_data = base::MakeRefCounted<hls::MediaSegment::EncryptionData>(
          GURL("https://example.com/enc.key"), hls::XKeyTagMethod::kAES128,
          hls::XKeyTagKeyFormat::kIdentity, std::make_tuple(0, 0));
    }
    return base::MakeRefCounted<hls::MediaSegment>(
        base::Seconds(1), 0, 0, GURL("https://example.com"), std::move(init),
        std::move(enc_data), ByteRangeFromTuple(byte_range), std::nullopt,
        false, false, init_mode == InitMode::kPresent, false);
  }

 protected:
  std::unique_ptr<HlsNetworkAccessImpl> network_access_;
  raw_ptr<MockDataSourceFactory> factory_;
  std::unique_ptr<HlsDataSourceProviderImpl> dsp_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HlsNetworkAccessImplUnittest, TestReadSmallManifest) {
  // Expect a read from 0-16k, but the manifest is only 800. A second read
  // happens to confirm nothing left.
  factory_->AddReadExpectation(0, 16384, 800);
  factory_->AddReadExpectation(800, 16384, 0);

  network_access_->ReadManifest(
      GURL("example.com"),
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 800lu);
        ASSERT_EQ(stream->buffer_size(), 800lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadLargeManifest) {
  // Expect a read from 0-16k, and the manifest is 20k..
  factory_->AddReadExpectation(0, 16384, 16384);
  factory_->AddReadExpectation(16384, 16384, 3616);
  factory_->AddReadExpectation(20000, 16384, 0);

  network_access_->ReadManifest(
      GURL("example.com"),
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 20000lu);
        ASSERT_EQ(stream->buffer_size(), 20000lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadSimpleSegment) {
  auto segment = MakeSegment(std::nullopt, std::nullopt);

  // the whole stream is short, so only two reads.
  factory_->AddReadExpectation(0, 16384, 1000);
  factory_->AddReadExpectation(1000, 16384, 0);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([&](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 1000lu);
        ASSERT_EQ(stream->buffer_size(), 1000lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadLargerSegment) {
  auto segment = MakeSegment(std::nullopt, std::nullopt);
  // A longer stream may need more reads.
  factory_->AddReadExpectation(0, 16384, 16384);
  factory_->AddReadExpectation(16384, 16384, 16384);
  factory_->AddReadExpectation(32768, 16384, 16384);
  factory_->AddReadExpectation(49152, 16384, 1);
  factory_->AddReadExpectation(49153, 16384, 0);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 49153lu);
        ASSERT_EQ(stream->buffer_size(), 49153lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadSegmentWithInit) {
  // When there is an init segment, it'll make two requests each starting at 0
  // and then concatenate them
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kPresent);
  factory_->AddReadExpectation(0, 16384, 1000);
  factory_->AddReadExpectation(1000, 16384, 0);
  factory_->PregenerateNextMock();
  factory_->AddReadExpectation(0, 16384, 1000);
  factory_->AddReadExpectation(1000, 16384, 0);
  factory_->PregenerateNextMock();

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 1000lu);
        ASSERT_EQ(stream->buffer_size(), 2000lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadLongerInitSegment) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kPresent);
  // When there is an init segment, it'll make two requests each starting at 0
  // and then concatenate them
  factory_->AddReadExpectation(0, 16384, 16384);
  factory_->AddReadExpectation(16384, 16384, 100);
  factory_->AddReadExpectation(16484, 16384, 0);
  factory_->PregenerateNextMock();
  factory_->AddReadExpectation(0, 16384, 1000);
  factory_->AddReadExpectation(1000, 16384, 0);
  factory_->PregenerateNextMock();

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 1000lu);
        ASSERT_EQ(stream->buffer_size(), 17484lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithSmallRange) {
  auto segment = MakeSegment(std::make_tuple(100, 100), std::nullopt);
  // the whole stream is short, so only two reads.
  factory_->AddReadExpectation(100, 100, 100);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([&](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 200lu);
        ASSERT_EQ(stream->buffer_size(), 100lu);
        ASSERT_EQ(stream->max_read_position(), 200lu);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithLargeRange) {
  auto segment = MakeSegment(std::make_tuple(100000, 100), std::nullopt);
  // the whole stream is short, so only two reads.
  factory_->AddReadExpectation(100, 16384, 16384);
  factory_->AddReadExpectation(16484, 16384, 16384);
  factory_->AddReadExpectation(32868, 16384, 16384);
  factory_->AddReadExpectation(49252, 16384, 16384);
  factory_->AddReadExpectation(65636, 16384, 16384);
  factory_->AddReadExpectation(82020, 16384, 16384);
  factory_->AddReadExpectation(98404, 1696, 1696);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce([&](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 100100lu);
        ASSERT_EQ(stream->buffer_size(), 100000lu);
        ASSERT_EQ(stream->max_read_position(), 100100lu);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentReadNoChunk) {
  auto segment = MakeSegment(std::nullopt, std::make_tuple(100000, 100),
                             InitMode::kPresent);
  factory_->AddReadExpectation(100, 16384, 16384);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init=*/true,
      base::BindOnce([&](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 16484lu);
        ASSERT_EQ(stream->buffer_size(), 16384lu);
        ASSERT_EQ(stream->max_read_position(), 100100lu);
        ASSERT_TRUE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithKey) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             KeyMode::kPresent);

  // This actually has to be 16 non-zero bytes.
  auto* ds_for_keyfetch = factory_->PregenerateNextMock();
  EXPECT_CALL(*ds_for_keyfetch, Initialize)
      .WillOnce(base::test::RunOnceCallback<0>(true));
  EXPECT_CALL(*ds_for_keyfetch, Read(0, 16384, _, _))
      .WillOnce([](int64_t, int, uint8_t* data, DataSource::ReadCB cb) {
        memset(data, 'x', 16);
        std::move(cb).Run(16);
      });
  EXPECT_CALL(*ds_for_keyfetch, Read(16, 16384, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(0));

  // Then expect media content to be read.
  factory_->AddReadExpectation(0, 16384, 1000);
  factory_->AddReadExpectation(1000, 16384, 0);

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init=*/true,
      base::BindOnce(
          [&](scoped_refptr<hls::MediaSegment> segment,
              HlsDataSourceProvider::ReadResult result) {
            ASSERT_TRUE(result.has_value());
            auto stream = std::move(result).value();
            ASSERT_EQ(stream->read_position(), 1000lu);
            ASSERT_EQ(stream->buffer_size(), 1000lu);
            ASSERT_EQ(stream->max_read_position(), std::nullopt);
            ASSERT_FALSE(stream->CanReadMore());
            ASSERT_NE(segment->GetEncryptionData(), nullptr);
            ASSERT_FALSE(segment->GetEncryptionData()->NeedsKeyFetch());
          },
          segment));
  task_environment_.RunUntilIdle();
}

}  // namespace media
