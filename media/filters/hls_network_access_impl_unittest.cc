// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_network_access_impl.h"

#include "base/compiler_specific.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_rendition_impl.h"
#include "media/filters/hls_test_helpers.h"

namespace media {

namespace {

enum class InitMode {
  kPresent,
  kAbsent,
};

}  // namespace

using testing::_;
using testing::Return;

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

  void TearDown() override {
    factory_ = nullptr;
    network_access_.reset();
    task_environment_.RunUntilIdle();
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
      std::optional<std::string> key_location = std::nullopt) {
    scoped_refptr<hls::MediaSegment::InitializationSegment> init = nullptr;
    scoped_refptr<hls::MediaSegment::EncryptionData> enc_data = nullptr;
    if (init_mode == InitMode::kPresent) {
      init = base::MakeRefCounted<hls::MediaSegment::InitializationSegment>(
          GURL("https://foo.com"), ByteRangeFromTuple(init_br));
    }
    auto manifest_uri = GURL("https://example.com/manifest.m3u8");
    auto resource_uri = GURL("https://example.com/content.mp4");
    if (key_location.has_value()) {
      auto key_uri = GURL(*key_location);
      enc_data = base::MakeRefCounted<hls::MediaSegment::EncryptionData>(
          GURL(*key_location), hls::XKeyTagMethod::kAES128,
          hls::XKeyTagKeyFormat::kIdentity, std::make_tuple(0, 0));
    }
    return base::MakeRefCounted<hls::MediaSegment>(
        base::Seconds(1), 0, 0, resource_uri, url::Origin::Create(manifest_uri),
        std::move(init), std::move(enc_data), ByteRangeFromTuple(byte_range),
        std::nullopt, false, false, init_mode == InitMode::kPresent, false,
        std::nullopt);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HlsNetworkAccessImpl> network_access_;
  raw_ptr<MockDataSourceFactory> factory_;
};

TEST_F(HlsNetworkAccessImplUnittest, TestReadSmallManifest) {
  // Expect a read from 0-16k, but the manifest is only 800. A second read
  // happens to confirm nothing left.
  factory_->AddReadExpectation(0, 16384, 800);
  factory_->AddReadExpectation(800, 16384, 0);

  network_access_->ReadManifest(
      GURL("https://example.com"),
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
      GURL("https://example.com"),
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
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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
  ASSERT_TRUE(segment->GetInitializationSegment());
  const GURL& init_uri = segment->GetInitializationSegment()->GetUri();
  const GURL& media_uri = segment->GetUri();

  factory_->AddReadExpectation(init_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(init_uri, 1000, 16384, 0);
  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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
  ASSERT_TRUE(segment->GetInitializationSegment());
  const GURL& init_uri = segment->GetInitializationSegment()->GetUri();
  const GURL& media_uri = segment->GetUri();

  // When there is an init segment, it'll make two requests each starting at 0
  // and then concatenate them
  factory_->AddReadExpectation(init_uri, 0, 16384, 16384);
  factory_->AddReadExpectation(init_uri, 16384, 16384, 100);
  factory_->AddReadExpectation(init_uri, 16484, 16384, 0);
  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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

  const GURL init_uri("https://foo.com");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, init_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(100, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(100));
        EXPECT_CALL(*mock, Read(200, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(500));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce([&](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 500lu);
        ASSERT_EQ(stream->buffer_size(), 16484lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_TRUE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithKey) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.com/enc.key");

  const GURL key_uri("https://example.com/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(
                [](int64_t, base::span<uint8_t> data, DataSource::ReadCB cb) {
                  std::ranges::fill(data.first<16>(), 'x');
                  std::move(cb).Run(16);
                });
        EXPECT_CALL(*mock, Read(16, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(true));
      });

  // Then expect media content to be read.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _)).Times(1);
  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
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

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithCORSKey) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.net/enc.key");

  const GURL key_uri("https://example.net/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(
                [](int64_t, base::span<uint8_t> data, DataSource::ReadCB cb) {
                  std::ranges::fill(data.first<16>(), 'x');
                  std::move(cb).Run(16);
                });
        EXPECT_CALL(*mock, Read(16, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(true));
      });

  // Then expect media content to be read.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _)).Times(1);
  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
      base::BindOnce(
          [&](scoped_refptr<hls::MediaSegment> segment,
              HlsDataSourceProvider::ReadResult result) {
            // The key was hosted on example.net while the manifest is hosted
            // on example.com. The example.net request did not provide a
            // Access-Control-Allow-Origin header in it's request, so the use
            // of the key is blocked, and the segment cannot be decrypted.
            ASSERT_FALSE(result.has_value());
          },
          segment));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, SegmentWithRedirectedManifest) {
  // The manifest, segment, and key are all hosted on https://example.com.
  // The manifest request triggers a redirect to a https://example.net
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.com/enc.key");

  const GURL key_uri("https://example.com/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(
                [](int64_t, base::span<uint8_t> data, DataSource::ReadCB cb) {
                  std::ranges::fill(data.first<16>(), 'x');
                  std::move(cb).Run(16);
                });
        EXPECT_CALL(*mock, Read(16, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        const GURL redirected_media_uri("https://example.net/content.mp4");
        MockDataSourceFactory::ConfigureAsRedirect(mock, redirected_media_uri);
        // The redirected data source will be read.
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(1000));
        EXPECT_CALL(*mock, Read(1000, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(true));
      });

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());

  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
      base::BindOnce(
          [&](scoped_refptr<hls::MediaSegment> segment,
              HlsDataSourceProvider::ReadResult result) {
            // The media data was not hosted on the same origin as the
            // manifest, and had origin tainting. This is unacceptable for
            // security reasons.
            ASSERT_FALSE(result.has_value());
          },
          segment));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentWithRedirectionKey) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.com/enc.key");

  const GURL key_uri("https://example.com/enc.key");
  const GURL redirected_key_uri("https://crypto-r-us.net/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([redirected_key_uri](MockDataSource* mock, const GURL& uri,
                                     ...) {
        MockDataSourceFactory::ConfigureAsRedirect(mock, redirected_key_uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(
                [](int64_t, base::span<uint8_t> data, DataSource::ReadCB cb) {
                  std::ranges::fill(data.first<16>(), 'x');
                  std::move(cb).Run(16);
                });
        EXPECT_CALL(*mock, Read(16, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        // It redirects to crypto-r-us.net, which taints origin.
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(true));
      });

  // Then expect media content to be read.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _)).Times(1);
  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
      base::BindOnce(
          [&](scoped_refptr<hls::MediaSegment> segment,
              HlsDataSourceProvider::ReadResult result) {
            // The key was hosted on example.com but performed a redirect. After
            // redirection, the request did not present an
            // Access-Control-Allow-Origin header, so the use of the key is
            // blocked, and the segment cannot be decrypted.
            ASSERT_FALSE(result.has_value());
          },
          segment));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadManifestAllowsGzip) {
  factory_->AddReadExpectation(0, 16384, 800);
  factory_->AddReadExpectation(800, 16384, 0);

  EXPECT_CALL(*factory_, Setup(_, GURL("https://example.com/manifest.m3u8"),
                               DataSource::CacheMode::kBypassCache,
                               DataSource::EncodingMode::kAllowGzip))
      .Times(1);

  network_access_->ReadManifest(
      GURL("https://example.com/manifest.m3u8"),
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestReadKeyDisallowsGzip) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.com/enc.key");

  const GURL key_uri("https://example.com/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  EXPECT_CALL(*factory_, Setup(_, key_uri, DataSource::CacheMode::kHitCache,
                               DataSource::EncodingMode::kIdentity))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(
                [](int64_t, base::span<uint8_t> data, DataSource::ReadCB cb) {
                  std::ranges::fill(data.first<16>(), 'x');
                  std::move(cb).Run(16);
                });
        EXPECT_CALL(*mock, Read(16, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(0));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(true));
      });

  EXPECT_CALL(*factory_, Setup(_, media_uri, DataSource::CacheMode::kHitCache,
                               DataSource::EncodingMode::kIdentity))
      .Times(1);

  factory_->AddReadExpectation(media_uri, 0, 16384, 1000);
  factory_->AddReadExpectation(media_uri, 1000, 16384, 0);

  ASSERT_NE(segment->GetEncryptionData(), nullptr);
  ASSERT_TRUE(segment->GetEncryptionData()->NeedsKeyFetch());
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, /*include_init_segment=*/true,
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentReadInitConnectionFailure) {
  auto segment =
      MakeSegment(std::nullopt, std::make_tuple(100, 100), InitMode::kPresent);

  const GURL init_uri("https://foo.com");
  const GURL media_uri("https://example.com/content.mp4");

  // Init segment fails to connect
  EXPECT_CALL(*factory_, Setup(_, init_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsFailure(mock);
      });

  // Media segment succeeds to connect, and we mock its read.
  // Even if Init fails, Media might still be created and read in parallel.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(500));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  bool callback_called = false;
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce(
          [](bool* cb_called, HlsDataSourceProvider::ReadResult result) {
            *cb_called = true;
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(std::move(result).error().code(),
                      HlsDemuxerStatus::Codes::kNetworkReadStopped);
          },
          &callback_called));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentReadMediaReadFailure) {
  auto segment =
      MakeSegment(std::nullopt, std::make_tuple(100, 100), InitMode::kPresent);

  const GURL init_uri("https://foo.com");
  const GURL media_uri("https://example.com/content.mp4");

  // Init segment succeeds
  EXPECT_CALL(*factory_, Setup(_, init_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(100, SpanSizeEq(100), _))
            .WillOnce(base::test::RunOnceCallback<2>(100));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  // Media segment fails read
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(DataSource::kReadError));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  bool callback_called = false;
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce(
          [](bool* cb_called, HlsDataSourceProvider::ReadResult result) {
            *cb_called = true;
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(std::move(result).error().code(),
                      HlsDemuxerStatus::Codes::kNetworkReadError);
          },
          &callback_called));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HlsNetworkAccessImplUnittest,
       TestSegmentReadInitReadFailureMediaSucceeds) {
  auto segment =
      MakeSegment(std::nullopt, std::make_tuple(100, 100), InitMode::kPresent);

  const GURL init_uri("https://foo.com");
  const GURL media_uri("https://example.com/content.mp4");

  // Init segment fails read
  EXPECT_CALL(*factory_, Setup(_, init_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(100, SpanSizeEq(100), _))
            .WillOnce(base::test::RunOnceCallback<2>(DataSource::kReadError));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  // Media segment succeeds
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(500));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  bool callback_called = false;
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce(
          [](bool* cb_called, HlsDataSourceProvider::ReadResult result) {
            *cb_called = true;
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(std::move(result).error().code(),
                      HlsDemuxerStatus::Codes::kNetworkReadError);
          },
          &callback_called));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentReadKeyFailure) {
  auto segment = MakeSegment(std::nullopt, std::nullopt, InitMode::kAbsent,
                             "https://example.com/enc.key");

  const GURL key_uri("https://example.com/enc.key");
  const GURL media_uri("https://example.com/content.mp4");

  // Key segment fails read
  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(DataSource::kReadError));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  // Media segment succeeds (but it might be aborted/ignored after key fails)
  // Actually, they start in parallel. Key and Media.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(500));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  bool callback_called = false;
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce(
          [](bool* cb_called, HlsDataSourceProvider::ReadResult result) {
            *cb_called = true;
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(std::move(result).error().code(),
                      HlsDemuxerStatus::Codes::kNetworkReadError);
          },
          &callback_called));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HlsNetworkAccessImplUnittest, TestSegmentReadKeyFailureLater) {
  auto segment = MakeSegment(std::nullopt, std::make_tuple(100, 100),
                             InitMode::kPresent, "https://example.com/enc.key");

  const GURL init_uri("https://foo.com");
  const GURL media_uri("https://example.com/content.mp4");
  const GURL key_uri("https://example.com/enc.key");

  DataSource::ReadCB key_read_cb;

  // Key segment Setup. It will capture the ReadCB and NOT run it immediately.
  EXPECT_CALL(*factory_, Setup(_, key_uri, _, _))
      .WillOnce([&](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce([&](int64_t, base::span<uint8_t>, DataSource::ReadCB cb) {
              key_read_cb = std::move(cb);
            });
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  // Init segment Setup. Succeeds synchronously.
  EXPECT_CALL(*factory_, Setup(_, init_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(100, SpanSizeEq(100), _))
            .WillOnce(base::test::RunOnceCallback<2>(100));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  // Media segment Setup. Succeeds synchronously.
  EXPECT_CALL(*factory_, Setup(_, media_uri, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(500));
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillRepeatedly(testing::Return(false));
      });

  bool callback_called = false;
  network_access_->ReadMediaSegment(
      *segment, /*read_chunked=*/true, /*include_init_segment=*/true,
      base::BindOnce(
          [](bool* cb_called, HlsDataSourceProvider::ReadResult result) {
            *cb_called = true;
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(std::move(result).error().code(),
                      HlsDemuxerStatus::Codes::kNetworkReadError);
          },
          &callback_called));

  // Run until idle. This will run Key Setup (capturing callback),
  // and run Init and Media Setup and their reads to completion.
  task_environment_.RunUntilIdle();

  // The overall callback should NOT have run yet because Key is still pending.
  EXPECT_FALSE(callback_called);
  ASSERT_TRUE(key_read_cb);

  // Now fail the key read.
  std::move(key_read_cb).Run(DataSource::kReadError);

  // Run until idle again to process the key failure and trigger overall
  // callback.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

}  // namespace media
