// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_rendition_impl.h"

#include <string_view>

#include "base/strings/string_view_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "crypto/random.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_network_access_impl.h"
#include "media/filters/hls_test_helpers.h"

namespace media {

namespace {

constexpr char kInitialFetchPlaylist[] =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:14551245\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551245.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551246.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551247.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551248.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551249.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551250.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551251.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551252.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551253.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551254.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551255.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551256.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551257.ts\n";

const std::string kSecondFetchLivePlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:14551349\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551349.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551350.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551351.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551352.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551353.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551354.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551355.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551356.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551357.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551358.ts\n";

constexpr char kInitialFetchLongPlaylist[] =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-MEDIA-SEQUENCE:14551245\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551245.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551246.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551247.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551248.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551249.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551250.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551251.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551252.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551253.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551254.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551255.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551256.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551257.ts\n";

const std::string kSecondFetchLiveLongPlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-MEDIA-SEQUENCE:14551349\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551349.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551350.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551351.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551352.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551353.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551354.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551355.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551356.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551357.ts\n"
    "#EXTINF:10.00000,\n"
    "playlist_4500Kb_14551358.ts\n";

const std::string kAESContent =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"key1.enc\",IV="
    "0x66666666666666666666666666666666\n"
    "#EXTINF:2.00000,\n"
    "media_0.ts\n"
    "#EXTINF:2.00000,\n"
    "media_1.ts\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"key2.enc\",IV="
    "0x67676767676767676767676767676767\n"
    "#EXTINF:2.00000,\n"
    "media_2.ts\n"
    "#EXTINF:2.00000,\n"
    "media_3.ts\n"
    "#EXTINF:2.00000,\n"
    "media_4.ts\n"
    "#EXTINF:2.00000,\n"
    "media_5.ts\n"
    "#EXT-X-ENDLIST\n";

const std::string kAESContentReplacement =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"keyx1.enc\",IV="
    "0x66666666666666666666666666666666\n"
    "#EXTINF:2.00000,\n"
    "mediax_0.ts\n"
    "#EXTINF:2.00000,\n"
    "mediax_1.ts\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"keyx2.enc\",IV="
    "0x68686868686868686868686868686868\n"
    "#EXTINF:2.00000,\n"
    "mediax_2.ts\n"
    "#EXTINF:2.00000,\n"
    "mediax_3.ts\n"
    "#EXTINF:2.00000,\n"
    "mediax_4.ts\n"
    "#EXTINF:2.00000,\n"
    "mediax_5.ts\n"
    "#EXT-X-ENDLIST\n";

const std::string kDiscontinuous =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-PLAYLIST-TYPE:VOD\n"
    "#EXT-X-INDEPENDENT-SEGMENTS\n"
    "#EXTINF:2.000000,\n"
    "bip00.ts\n"
    "#EXTINF:2.000000,\n"
    "bip01.ts\n"
    "#EXTINF:2.000000,\n"
    "bip02.ts\n"
    "#EXT-X-DISCONTINUITY\n"
    "#EXTINF:1.600000,\n"
    "data00.ts\n"
    "#EXTINF:1.600000,\n"
    "data01.ts\n"
    "#EXTINF:1.600000,\n"
    "data02.ts\n"
    "#EXTINF:1.600000,\n"
    "data03.ts\n"
    "#EXT-X-ENDLIST\n";

const std::string kSingleSegmentPlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:14551245\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551245.ts\n";

const std::string kGapPlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-PLAYLIST-TYPE:VOD\n"
    "#EXTINF:2.00000,\n"
    "media_0.ts\n"
    "#EXT-X-GAP\n"
    "#EXTINF:2.00000,\n"
    "media_1.ts\n"
    "#EXTINF:2.00000,\n"
    "media_2.ts\n"
    "#EXT-X-ENDLIST\n";

}  // namespace

using testing::_;
using testing::ElementsAreArray;
using testing::NiceMock;
using testing::Return;

MATCHER_P(MediaSegmentHasUrl, urlstr, "MediaSegment has provided URL") {
  return arg.GetUri() == GURL(urlstr);
}

class HlsRenditionImplUnittest : public testing::Test {
 protected:
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsRenditionHost> mock_hrh_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HlsRenditionImpl> MakeVodRendition(std::string_view content) {
    constexpr hls::types::DecimalInteger version = 3;
    auto uri = GURL("https://example.com/manifest.m3u8");
    auto parsed = hls::MediaPlaylist::Parse(content, uri, version, nullptr);
    if (!parsed.has_value()) {
      LOG(ERROR) << MediaSerializeForTesting(std::move(parsed).error());
      return nullptr;
    }
    auto playlist = std::move(parsed).value();
    auto duration = playlist->GetComputedDuration();
    media_log_ = std::make_unique<NiceMock<media::MockMediaLog>>();

    return std::make_unique<HlsRenditionImpl>(mock_mdeh_.get(), mock_hrh_.get(),
                                              "test", std::move(playlist),
                                              duration, uri, media_log_.get());
  }

  std::unique_ptr<HlsRenditionImpl> MakeLiveRendition(
      GURL uri,
      std::string_view content) {
    constexpr hls::types::DecimalInteger version = 3;
    auto parsed = hls::MediaPlaylist::Parse(content, uri, version, nullptr);
    if (!parsed.has_value()) {
      LOG(ERROR) << MediaSerializeForTesting(std::move(parsed).error());
      return nullptr;
    }
    media_log_ = std::make_unique<NiceMock<media::MockMediaLog>>();
    return std::make_unique<HlsRenditionImpl>(
        mock_mdeh_.get(), mock_hrh_.get(), "test", std::move(parsed).value(),
        std::nullopt, uri, media_log_.get());
  }

  MOCK_METHOD(void, CheckStateComplete, (base::TimeDelta delay), ());

  ManifestDemuxer::DelayCallback BindCheckState(base::TimeDelta time) {
    EXPECT_CALL(*this, CheckStateComplete(time));
    return base::BindOnce(&HlsRenditionImplUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  ManifestDemuxer::DelayCallback BindCheck0Sec() {
    EXPECT_CALL(*this, CheckStateComplete(base::Seconds(0)));
    return base::BindOnce(&HlsRenditionImplUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  ManifestDemuxer::DelayCallback BindCheckStateNoExpect() {
    return base::BindOnce(&HlsRenditionImplUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  void RequireAppend(base::span<const uint8_t> data, bool return_value = true) {
    EXPECT_CALL(*mock_mdeh_,
                AppendAndParseData(_, _, _, base::as_byte_span(data)))
        .WillOnce(Return(return_value));
  }

  void RespondWithRange(base::TimeDelta start, base::TimeDelta end) {
    Ranges<base::TimeDelta> ranges;
    if (start != end) {
      ranges.Add(start, end);
    }
    EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
        .WillRepeatedly(Return(ranges));
  }

  void RespondWithRangeTwice(base::TimeDelta A,
                             base::TimeDelta B,
                             base::TimeDelta X,
                             base::TimeDelta Y) {
    Ranges<base::TimeDelta> ab;
    if (A != B) {
      ab.Add(A, B);
    }
    Ranges<base::TimeDelta> xy;
    if (X != Y) {
      xy.Add(X, Y);
    }
    EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
        .WillOnce(Return(ab))
        .WillOnce(Return(xy));
  }

  void SupplyAndExpectJunkData(base::TimeDelta initial_response_start,
                               base::TimeDelta initial_response_end,
                               base::TimeDelta fetch_expected_time) {
    std::string junk_content = "abcdefg, I dont like to sing rhyming songs";
    EXPECT_CALL(*mock_hrh_, ReadMediaSegment(_, _, _, _))
        .WillOnce([content = junk_content, host = mock_hrh_.get()](
                      const hls::MediaSegment&, bool, bool,
                      HlsDataSourceProvider::ReadCb cb) {
          auto stream = StringHlsDataSourceStreamFactory::CreateStream(content);
          std::move(cb).Run(std::move(stream));
        });
    EXPECT_CALL(
        *mock_mdeh_,
        AppendAndParseData("test", _, _,
                           ElementsAreArray(base::as_byte_span(junk_content))))
        .WillOnce(Return(true));
    Ranges<base::TimeDelta> initial_range;
    Ranges<base::TimeDelta> appended_range;
    if (initial_response_end != initial_response_start) {
      initial_range.Add(initial_response_start, initial_response_end);
    }
    appended_range.Add(fetch_expected_time - base::Seconds(1),
                       fetch_expected_time + base::Seconds(1));
    EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
        .Times(2)
        .WillOnce(Return(initial_range))
        .WillOnce(Return(appended_range));
  }

  void RespondToUrl(std::string uri, std::string content) {
    EXPECT_CALL(*mock_hrh_, UpdateNetworkSpeed(_));
    EXPECT_CALL(*mock_hrh_, ReadMediaSegment(MediaSegmentHasUrl(uri), _, _, _))
        .WillOnce([content = std::move(content), host = mock_hrh_.get()](
                      const hls::MediaSegment& segment, bool, bool,
                      HlsDataSourceProvider::ReadCb cb) {
          if (auto enc_data = segment.GetEncryptionData()) {
            ASSERT_FALSE(enc_data->NeedsKeyFetch());
          }
          auto stream = StringHlsDataSourceStreamFactory::CreateStream(content);
          std::move(cb).Run(std::move(stream));
        });
  }

  void InterceptEncDataOnFetch(
      std::string uri,
      std::string content,
      base::OnceCallback<void(hls::MediaSegment::EncryptionData*)> intercept) {
    EXPECT_CALL(*mock_hrh_, UpdateNetworkSpeed(_));
    EXPECT_CALL(*mock_hrh_, ReadMediaSegment(MediaSegmentHasUrl(uri), _, _, _))
        .WillOnce([content = std::move(content), host = mock_hrh_.get(),
                   intercept = std::move(intercept)](
                      const hls::MediaSegment& segment, bool, bool,
                      HlsDataSourceProvider::ReadCb cb) mutable {
          if (auto enc_data = segment.GetEncryptionData()) {
            ASSERT_TRUE(enc_data->NeedsKeyFetch());
            std::move(intercept).Run(enc_data.get());
          }
          auto stream = StringHlsDataSourceStreamFactory::CreateStream(content);
          std::move(cb).Run(std::move(stream));
        });
  }

  static constexpr size_t kKeySize = 16;
  std::tuple<std::string, std::array<uint8_t, kKeySize>> Encrypt(
      std::string cleartext,
      base::span<const uint8_t, crypto::aes_cbc::kBlockSize> iv) {
    std::array<uint8_t, kKeySize> key;
    crypto::RandBytes(key);
    auto ciphertext =
        crypto::aes_cbc::Encrypt(key, iv, base::as_byte_span(cleartext));
    return std::make_tuple(std::string(base::as_string_view(ciphertext)), key);
  }

 public:
  HlsRenditionImplUnittest()
      : mock_mdeh_(std::make_unique<MockManifestDemuxerEngineHost>()),
        mock_hrh_(std::make_unique<MockHlsRenditionHost>()) {
    EXPECT_CALL(*mock_mdeh_, RemoveRole("test"));
  }
};

TEST_F(HlsRenditionImplUnittest, TestCheckStateFromNoData) {
  auto rendition = MakeVodRendition(kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);

  SupplyAndExpectJunkData(base::Seconds(0), base::Seconds(0), base::Seconds(1));
  rendition->CheckState(base::Seconds(0), 1.0,
                        BindCheckState(base::Seconds(0)));

  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestCheckStateWithLargeBufferCached) {
  auto rendition = MakeVodRendition(kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);

  // Prime the download speed cache.
  SupplyAndExpectJunkData(base::Seconds(0), base::Seconds(0), base::Seconds(1));
  rendition->CheckState(base::Seconds(0), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // This time respond with a large range of loaded data.
  // Time until underflow is going to be 12 seconds here - the fetch time
  // average is zero, since this is a unittest, and we subtract 5 seconds flag
  // giving a delay of 7 seconds.
  RespondWithRange(base::Seconds(0), base::Seconds(12));
  rendition->CheckState(base::Seconds(0), 1.0,
                        BindCheckState(base::Seconds(7)));

  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestCheckStateWithTooLateBuffer) {
  auto rendition = MakeVodRendition(kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);

  RespondWithRange(base::Seconds(10), base::Seconds(12));
  EXPECT_CALL(*mock_hrh_, Quit(_));
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckStateNoExpect());

  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestStop) {
  auto rendition = MakeVodRendition(kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);

  rendition->Stop();

  // Should always be kNoTimestamp after `Stop()` and no network requests.
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
}

TEST_F(HlsRenditionImplUnittest, TestNonRealTimePlaybackRate) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // Any rate not 0.0 or 1.0 should error.
  EXPECT_CALL(*mock_hrh_, Quit(_));
  rendition->CheckState(base::Seconds(0), 2.0, BindCheckStateNoExpect());
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestCreateRenditionPaused) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // CheckState causes the rendition to:
  // Check buffered ranges first
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(5));
  // The first segment will be queried
  std::string tscontent = "tscontent";
  RespondToUrl("http://example.com/playlist_4500Kb_14551255.ts", tscontent);
  // Then appended.
  EXPECT_CALL(*mock_mdeh_,
              AppendAndParseData(_, _, _, base::as_byte_span(tscontent)))
      .WillOnce(Return(true));
  // CheckState should in this case respond with a delay of zero seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestPausedRenditionHasSomeData) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 8)
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(8), base::Seconds(0),
                        base::Seconds(16));

  // The next unqueried segment will be queried
  std::string tscontent = "tscontent";
  RespondToUrl("http://example.com/playlist_4500Kb_14551255.ts", tscontent);
  // Then appended.
  EXPECT_CALL(*mock_mdeh_,
              AppendAndParseData(_, _, _, base::as_byte_span(tscontent)))
      .WillOnce(Return(true));
  // CheckState should in this case respond with a delay of zero seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestPausedRenditionHasEnoughBufferedData) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 12)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(12));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .Times(2)
      .WillRepeatedly(Return(loaded_ranges));
  // Old data will try to be removed. Since media time is 0, there is nothing
  // to do. Then there will be an attempt to fetch a new manifest, which won't
  // have any work to do either, instead just posting the delay_cb back.
  // CheckState should in this case respond with a delay of 12 - 10 / 2 seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(7)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestRenditionHasEnoughDataFetchNewManifest) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 12)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(12));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .Times(2)
      .WillRepeatedly(Return(loaded_ranges));
  // Old data will try to be removed. Since media time is 0, there is nothing
  // to do. Then there will be an attempt to fetch a new manifest, which will
  // get an update.
  task_environment_.FastForwardBy(base::Seconds(33));
  EXPECT_CALL(*mock_hrh_,
              UpdateRenditionManifestUri("test", GURL("http://example.com"), _))
      .WillOnce([](std::string role, GURL uri, HlsDemuxerStatusCallback cb) {
        std::move(cb).Run(OkStatus());
      });

  // CheckState should in this case respond with a delay of 12 - 10/2 seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(7)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestRenditionHasEnoughDataDeleteOldContent) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 42)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(42));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .Times(2)
      .WillRepeatedly(Return(loaded_ranges));
  // We will remove old data here - which is max(10, 2*segment_duration) behind
  // the current timestamp, or 25 - max(10, 4) = 15 seconds
  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(15)));
  task_environment_.FastForwardBy(base::Seconds(25));

  // There are only three segments (6 seconds) left in the buffer, so we'll
  // pull for manifest updates.
  EXPECT_CALL(*mock_hrh_, UpdateRenditionManifestUri("test", _, _))
      .WillOnce([&rendition](std::string role, GURL uri,
                             HlsDemuxerStatusCallback cb) {
        auto parsed = hls::MediaPlaylist::Parse(
            kSecondFetchLivePlaylist, GURL("http://example.com"), 3, nullptr);
        CHECK(parsed.has_value());
        rendition->UpdatePlaylist(std::move(parsed).value());
        std::move(cb).Run(OkStatus());
      });

  // CheckState should in this case respond with a delay of 17 - 10 / 2 seconds.
  rendition->CheckState(base::Seconds(25), 0.0,
                        BindCheckState(base::Seconds(12)));

  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestStopLive) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchPlaylist);
  ASSERT_NE(rendition, nullptr);

  rendition->Stop();

  // Should always be kNoTimestamp after `Stop()` and no network requests.
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
}

TEST_F(HlsRenditionImplUnittest, TestPauseAndUnpause) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLongPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  ON_CALL(*mock_mdeh_, OnError(_)).WillByDefault([](PipelineStatus st) {
    LOG(ERROR) << MediaSerializeForTesting(st);
  });

  // CheckState will start with a paused player. It will query BufferedRanges
  // for the CheckState function, then try to fetch. This will pop the first
  // segment and try to load it. This will then get appended, and ranges will
  // be checked again. It will report 2 seconds of content, which contains
  // the media time (0.0), and so a response to check state again in 0 seconds
  // will happen.
  std::string tscontent = "tscontent";
  RespondToUrl("http://example.com/playlist_4500Kb_14551255.ts", tscontent);
  RequireAppend(base::as_byte_span(tscontent));
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // After the init process finishes, lets pretend there are 32 seconds of data
  // in the buffer. A user presses play after 9 second of the video being
  // paused. Rate goes to 1, and the delta between now and the pause timestamp
  // is 9 seconds, which is well within the duration of the manifest. The
  // rendition impl will request a seek to 9 seconds, and return no timestamp.
  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Seconds(9)));
  task_environment_.FastForwardBy(base::Seconds(9));

  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  // After the pipeline does it's seeking shenanigans, another check state
  // event will be called at 9 seconds, rate 1.0. Because there are 23 seconds
  // now left in the buffer, the response will be a requested pause of 18
  // seconds.
  RespondWithRange(base::Seconds(0), base::Seconds(32));
  rendition->CheckState(base::Seconds(9), 1.0,
                        BindCheckState(base::Seconds(18)));
  task_environment_.RunUntilIdle();

  // At 10 seconds the user will pause again, which will trigger another
  // state check. This will update the pause timestamp to 9 seconds, and because
  // we aren't in the initialization step, will return kNoTimestamp. Any other
  // state checks with a rate of 0 should also return no timestamp.
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  // Now the user waits for 190 seconds. Media time hasn't moved, so a seek
  // will be required. The segment queue will be reset, and old data will be
  // cleared (190 + mediatime (10) + segment_duration (10)). Then a new manifest
  // will be fetched, and the first segment will be loaded. The segment data
  // handler will check ranges to see if the time is found, and then the seek
  // handler will check ranges to get the new seek point. Then a seek will be
  // requested to the start of the range.
  std::string newcontent = "newcontent";
  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(210)));
  EXPECT_CALL(*mock_hrh_, UpdateRenditionManifestUri("test", _, _))
      .WillOnce([&rendition](std::string role, GURL uri,
                             HlsDemuxerStatusCallback cb) {
        auto parsed =
            hls::MediaPlaylist::Parse(kSecondFetchLiveLongPlaylist,
                                      GURL("http://example.com"), 3, nullptr);
        CHECK(parsed.has_value());
        rendition->UpdatePlaylist(std::move(parsed).value());
        std::move(cb).Run(OkStatus());
      });
  RespondToUrl("http://example.com/playlist_4500Kb_14551356.ts", newcontent);
  RequireAppend(base::as_byte_span(newcontent));
  RespondWithRangeTwice(base::Seconds(190), base::Seconds(202),
                        base::Seconds(190), base::Seconds(202));
  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Seconds(190)));
  task_environment_.FastForwardBy(base::Seconds(190));
  rendition->CheckState(base::Seconds(10), 1.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  // this time, the ranges are only 2 seconds past media time, so more data is
  // requested again, this time for the next segment. Lets pretend that next
  // segment has 20 seconds of data in it, bringing new range end to 222. The
  // response will still be 0 seconds.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(202), base::Seconds(0),
                        base::Seconds(222));
  newcontent = "blah";
  RespondToUrl("http://example.com/playlist_4500Kb_14551357.ts", "blah");
  RequireAppend(base::as_byte_span(newcontent));
  rendition->CheckState(base::Seconds(200), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // Now, finally, we've satisfied the buffer, so we can clear old segments,
  // and the loop can pause for (22 - 10/2) or 17 seconds.
  // Old data is 200 - max(10, 2*segment_duration), or 200 - max(10, 2*10) = 180

  RespondWithRange(base::Seconds(0), base::Seconds(222));
  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(180)));
  rendition->CheckState(base::Seconds(200), 1.0,
                        BindCheckState(base::Seconds(17)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestDiscontinuity) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  const std::string content = "ehh, whatever";

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/bip00.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/bip01.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/bip02.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/data00.ts", content);

  EXPECT_CALL(*mock_mdeh_, ResetParserState("test", kInfiniteDuration, _));

  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/data01.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();

  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  RespondToUrl("https://example.com/data02.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestAES128Content) {
  auto rendition = MakeVodRendition(kAESContent);

  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), base::Seconds(12));

  ON_CALL(*mock_mdeh_, OnError(_)).WillByDefault([](PipelineStatus st) {
    LOG(ERROR) << MediaSerializeForTesting(st);
  });

  std::string cleartext = "some kind of ts content.";
  std::string ciphertext;
  std::array<uint8_t, kKeySize> key;
  static constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kFIv{
      'f', 'f', 'f', 'f', 'f', 'f', 'f', 'f',
      'f', 'f', 'f', 'f', 'f', 'f', 'f', 'f',
  };
  std::tie(ciphertext, key) = Encrypt(cleartext, kFIv);

  /* START CHECK 1 */
  // CheckState will start the paused player, query BufferedRanges, get 0-0,
  // which will trigger an attempt to fetch.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));

  // The fetch will request the segment at "media_0.ts", which has an encryption
  // data attached. We need to populate that here so we can use the same key to
  // encrypt our plaintext.
  InterceptEncDataOnFetch(
      "https://example.com/media_0.ts", ciphertext,
      base::BindOnce(
          [](base::span<const uint8_t, kKeySize> key,
             hls::MediaSegment::EncryptionData* enc_data) {
            enc_data->ImportKey(std::string(base::as_string_view(key)));
          },
          key));

  // The cleartext will get appended, and the network speed will be updated
  // to some massive number.
  RequireAppend(base::as_byte_span(cleartext));

  // Ranges will be checked again, and we claim that it was data from 0-2.
  // Because our time (0) is within the range 0-2, CheckState will request a 0
  // second delay.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  /* START CHECK 2 */

  // On the second fetch, the encryption data should remain the same,
  // so we can just use new text.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(2), base::Seconds(0),
                        base::Seconds(4));
  RespondToUrl("https://example.com/media_1.ts", ciphertext);
  RequireAppend(base::as_byte_span(cleartext));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  /* START CHECK 3 */

  // There's a new key, and a new IV.
  std::string ciphertext2;

  static constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kGIv{
      'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g',
      'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g',
  };
  std::tie(ciphertext2, key) = Encrypt(cleartext, kGIv);

  // CheckState will start the paused player, query BufferedRanges, get 0-4
  // which will trigger an attempt to fetch.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(4), base::Seconds(0),
                        base::Seconds(6));

  // The fetch will request the segment at "media_2.ts", which has a new
  // encryption data.
  InterceptEncDataOnFetch(
      "https://example.com/media_2.ts", ciphertext2,
      base::BindOnce(
          [](base::span<const uint8_t, kKeySize> key,
             hls::MediaSegment::EncryptionData* enc_data) {
            enc_data->ImportKey(std::string(base::as_string_view(key)));
          },
          key));

  RequireAppend(base::as_byte_span(cleartext));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  /* START CHECK 4 */

  // Update the playlist. The segment stream should keep around media_3.ts,
  // but follow it up with mediax_4.ts
  GURL manifest_uri = GURL("https://example.com/manifest.m3u8");
  auto parsed = hls::MediaPlaylist::Parse(kAESContentReplacement, manifest_uri,
                                          3, nullptr);
  CHECK(parsed.has_value());
  rendition->UpdatePlaylist(std::move(parsed).value());

  // The encryption data is the same for segment 3, since it didn't change.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(2), base::Seconds(0),
                        base::Seconds(4));
  RespondToUrl("https://example.com/media_3.ts", ciphertext2);
  RequireAppend(base::as_byte_span(cleartext));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  /* START CHECK 5 */

  // This time mediax_4 doesn't have a new encryption segment, but it does need
  // a key fetch.

  std::string ciphertext3;
  static constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kHIv{
      'h', 'h', 'h', 'h', 'h', 'h', 'h', 'h',
      'h', 'h', 'h', 'h', 'h', 'h', 'h', 'h',
  };
  std::tie(ciphertext3, key) = Encrypt(cleartext, kHIv);

  // CheckState will start the paused player, query BufferedRanges, get 0-4
  // which will trigger an attempt to fetch.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(4), base::Seconds(0),
                        base::Seconds(6));

  // The fetch will request the segment at "mediax_4.ts", for which the
  // encryption data has not been fetched.
  InterceptEncDataOnFetch(
      "https://example.com/mediax_4.ts", ciphertext3,
      base::BindOnce(
          [](base::span<const uint8_t, kKeySize> key,
             hls::MediaSegment::EncryptionData* enc_data) {
            enc_data->ImportKey(std::string(base::as_string_view(key)));
          },
          key));
  RequireAppend(base::as_byte_span(cleartext));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  /* START CHECK 6 */

  // The encryption data is the same for segment 5, since it didn't change.
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(2), base::Seconds(0),
                        base::Seconds(4));
  RespondToUrl("https://example.com/mediax_5.ts", ciphertext3);
  RequireAppend(base::as_byte_span(cleartext));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestCanPlayWhenThereIsAGap) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> empty_range;
  Ranges<base::TimeDelta> split_range;
  split_range.Add(base::Seconds(0), base::Milliseconds(998));
  split_range.Add(base::Seconds(3), base::Milliseconds(3999));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(empty_range))   // CheckState #1
      .WillOnce(Return(split_range));  // OnSegmentData #1
  RespondToUrl("https://example.com/bip00.ts", content);
  RequireAppend(base::as_byte_span(content));
  rendition->CheckState(base::Seconds(0), 0.0, BindCheck0Sec());
}

TEST_F(HlsRenditionImplUnittest, TestCantSkipOverLargeGaps) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> split_range;
  split_range.Add(base::Seconds(0), base::Milliseconds(998));
  split_range.Add(base::Seconds(3), base::Milliseconds(3999));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(split_range));
  EXPECT_CALL(*mock_hrh_, Quit(_));
  rendition->CheckState(base::Milliseconds(999), 0.0, BindCheckStateNoExpect());
}

TEST_F(HlsRenditionImplUnittest, TestCantSkipIntoTinyRangeMiddle) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> split_range;
  Ranges<base::TimeDelta> truncated;
  split_range.Add(base::Milliseconds(10), base::Milliseconds(80));
  split_range.Add(base::Milliseconds(100), base::Milliseconds(102));
  split_range.Add(base::Milliseconds(104), base::Milliseconds(112));
  split_range.Add(base::Milliseconds(114), base::Milliseconds(130));
  split_range.Add(base::Milliseconds(132), base::Milliseconds(190));
  truncated.Add(base::Milliseconds(132), base::Milliseconds(190));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(split_range))
      .WillOnce(Return(truncated));

  EXPECT_CALL(*mock_mdeh_,
              Remove(_, base::Seconds(0), base::Milliseconds(130)));
  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Milliseconds(132)));
  rendition->CheckState(base::Milliseconds(90), 0.0,
                        BindCheckState(kNoTimestamp));
}

TEST_F(HlsRenditionImplUnittest, TestSkipsAheadIfBehind) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> split_range;
  split_range.Add(base::Milliseconds(100), base::Milliseconds(102));
  split_range.Add(base::Milliseconds(104), base::Milliseconds(112));
  split_range.Add(base::Milliseconds(114), base::Milliseconds(130));
  split_range.Add(base::Milliseconds(132), base::Milliseconds(190));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillRepeatedly(Return(split_range));

  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Milliseconds(100)));
  rendition->CheckState(base::Milliseconds(0), 1, BindCheckState(kNoTimestamp));
}

TEST_F(HlsRenditionImplUnittest, TestCantSkipIntoTheFarFuture) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> split_range;
  split_range.Add(base::Milliseconds(3100), base::Milliseconds(3102));
  split_range.Add(base::Milliseconds(3104), base::Milliseconds(3112));
  split_range.Add(base::Milliseconds(3114), base::Milliseconds(3130));
  split_range.Add(base::Milliseconds(3132), base::Milliseconds(3190));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(split_range));

  EXPECT_CALL(*mock_hrh_, Quit(_));
  rendition->CheckState(base::Milliseconds(0), 1, BindCheckStateNoExpect());
}

TEST_F(HlsRenditionImplUnittest, TestWillDelayUntilRangeWhenBufferFull) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  Ranges<base::TimeDelta> split_range;
  split_range.Add(base::Seconds(0), base::Seconds(2));
  split_range.Add(base::Seconds(3), base::Seconds(20));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillRepeatedly(Return(split_range));

  // The delay is set until the end of the first loaded range, because it is
  // closer than the "ideal buffer size"
  rendition->CheckState(base::Seconds(1), 1, BindCheckState(base::Seconds(1)));
}

TEST_F(HlsRenditionImplUnittest, TestRemoveOldDataForSkipRemovesAllBuffers) {
  auto rendition = MakeVodRendition(kDiscontinuous);
  ASSERT_NE(rendition, nullptr);
  std::string content = "123";
  Ranges<base::TimeDelta> split_range;
  Ranges<base::TimeDelta> truncated;
  split_range.Add(base::Milliseconds(10), base::Milliseconds(80));
  split_range.Add(base::Milliseconds(100), base::Milliseconds(102));
  split_range.Add(base::Milliseconds(104), base::Milliseconds(112));
  split_range.Add(base::Milliseconds(114), base::Milliseconds(130));
  split_range.Add(base::Milliseconds(132), base::Milliseconds(190));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(split_range))
      .WillOnce(Return(truncated));

  EXPECT_CALL(*mock_mdeh_,
              Remove(_, base::Seconds(0), base::Milliseconds(130)));
  EXPECT_CALL(*mock_hrh_, Quit(_));
  rendition->CheckState(base::Milliseconds(90), 0.0, BindCheckStateNoExpect());
}

TEST_F(HlsRenditionImplUnittest, SeekWithBadContentCausesError) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLongPlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), std::nullopt);

  ON_CALL(*mock_mdeh_, OnError(_)).WillByDefault([](PipelineStatus st) {
    LOG(ERROR) << MediaSerializeForTesting(st);
  });

  // CheckState will start with a paused player. It will query BufferedRanges
  // for the CheckState function, then try to fetch. This will pop the first
  // segment and try to load it. This will then get appended, and ranges will
  // be checked again. It will report 2 seconds of content, which contains
  // the media time (0.0), and so a response to check state again in 0 seconds
  // will happen.
  std::string tscontent = "tscontent";
  RespondToUrl("http://example.com/playlist_4500Kb_14551255.ts", tscontent);
  RequireAppend(base::as_byte_span(tscontent));
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // After the init process finishes, lets pretend there are 32 seconds of data
  // in the buffer. A user presses play after 9 second of the video being
  // paused. Rate goes to 1, and the delta between now and the pause timestamp
  // is 9 seconds, which is well within the duration of the manifest (20s). The
  // rendition impl will request a seek to 9 seconds, and return no timestamp.
  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Seconds(9)));
  task_environment_.FastForwardBy(base::Seconds(9));

  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  // After the pipeline does it's seeking shenanigans, another check state
  // event will be called at 9 seconds, rate 1.0. Because there are 23 seconds
  // now left in the buffer, the response will be a requested pause of 18
  // seconds.
  RespondWithRange(base::Seconds(0), base::Seconds(32));
  rendition->CheckState(base::Seconds(9), 1.0,
                        BindCheckState(base::Seconds(18)));
  task_environment_.RunUntilIdle();

  // At 10 seconds the user will pause again, which will trigger another
  // state check. This will update the pause timestamp to 9 seconds, and because
  // we aren't in the initialization step, will return kNoTimestamp. Any other
  // state checks with a rate of 0 should also return no timestamp.
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();
  rendition->CheckState(base::Seconds(10), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(210)));
  EXPECT_CALL(*mock_hrh_, UpdateRenditionManifestUri("test", _, _))
      .WillOnce([&rendition](std::string role, GURL uri,
                             HlsDemuxerStatusCallback cb) {
        auto parsed = hls::MediaPlaylist::Parse(
            kSingleSegmentPlaylist, GURL("http://example.com"), 3, nullptr);
        CHECK(parsed.has_value());
        rendition->UpdatePlaylist(std::move(parsed).value());
        std::move(cb).Run(OkStatus());
      });
  EXPECT_CALL(*mock_hrh_, Quit(_));
  task_environment_.FastForwardBy(base::Seconds(190));
  rendition->CheckState(base::Seconds(10), 1.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsRenditionImplUnittest, TestGapSegmentIsSkipped) {
  auto rendition = MakeVodRendition(kGapPlaylist);
  ASSERT_NE(rendition, nullptr);
  // First check should fetch the first segment.
  std::string content0 = "content0";
  RespondToUrl("https://example.com/media_0.ts", content0);
  RequireAppend(base::as_byte_span(content0));
  RespondWithRangeTwice(base::Seconds(0), base::Seconds(0), base::Seconds(0),
                        base::Seconds(2));
  rendition->CheckState(base::Seconds(0), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // The buffer is now [0, 2), which is less than the ideal 10s.
  // The next CheckState should try to fill the buffer.
  // It will encounter a GAP segment, skip it, and fetch the next one.
  EXPECT_CALL(
      *mock_hrh_,
      ReadMediaSegment(MediaSegmentHasUrl("https://example.com/media_1.ts"), _,
                       _, _))
      .Times(0);

  std::string content2 = "content2";
  RespondToUrl("https://example.com/media_2.ts", content2);
  RequireAppend(base::as_byte_span(content2));
  Ranges<base::TimeDelta> after_0;
  after_0.Add(base::Seconds(0), base::Seconds(2));
  Ranges<base::TimeDelta> after_2;
  after_2.Add(base::Seconds(0), base::Seconds(2));
  after_2.Add(base::Seconds(4), base::Seconds(6));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
      .WillOnce(Return(after_0))
      .WillOnce(Return(after_2));
  rendition->CheckState(base::Seconds(1), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();
}

}  // namespace media
