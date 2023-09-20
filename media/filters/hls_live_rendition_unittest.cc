// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_live_rendition.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_test_helpers.h"

namespace media {

namespace {

const std::string kInitialFetchLivePlaylist =
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
    "playlist_4500Kb_14551254.ts\n";

const std::string kSecondFetchLivePlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:14551249\n"
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
    "playlist_4500Kb_14551257.ts\n"
    "#EXTINF:2.00000,\n"
    "playlist_4500Kb_14551258.ts\n";

}  // namespace

using testing::_;
using testing::Return;

class HlsLiveRenditionUnittest : public testing::Test {
 protected:
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsRenditionHost> mock_hrh_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HlsLiveRendition> MakeLiveRendition(
      GURL uri,
      base::StringPiece content) {
    constexpr hls::types::DecimalInteger version = 3;
    auto parsed = hls::MediaPlaylist::Parse(content, uri, version, nullptr);
    if (!parsed.has_value()) {
      LOG(ERROR) << MediaSerialize(std::move(parsed).error());
      return nullptr;
    }
    return std::make_unique<HlsLiveRendition>(mock_mdeh_.get(), mock_hrh_.get(),
                                              "test", std::move(parsed).value(),
                                              uri);
  }

  MOCK_METHOD(void, CheckStateComplete, (base::TimeDelta delay), ());

  ManifestDemuxer::DelayCallback BindCheckState(base::TimeDelta time) {
    EXPECT_CALL(*this, CheckStateComplete(time));
    return base::BindOnce(&HlsLiveRenditionUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  ManifestDemuxer::DelayCallback BindCheckStateNoExpect() {
    return base::BindOnce(&HlsLiveRenditionUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  void RespondToUrl(std::string uri,
                    std::string content,
                    bool batching = true) {
    EXPECT_CALL(*mock_hrh_, ReadFromUrl(GURL(uri), batching, _, _))
        .WillOnce([content = std::move(content), host = mock_hrh_.get()](
                      GURL url, bool, absl::optional<hls::types::ByteRange>,
                      HlsDataSourceStreamManager::ReadCb cb) {
          auto ds = std::make_unique<StringHlsDataSource>(content);
          auto stream = std::make_unique<HlsDataSourceStream>(std::move(ds));
          host->ReadStream(std::move(stream), std::move(cb));
        });
  }

 public:
  HlsLiveRenditionUnittest()
      : mock_mdeh_(std::make_unique<MockManifestDemuxerEngineHost>()),
        mock_hrh_(std::make_unique<MockHlsRenditionHost>()) {}
};

TEST_F(HlsLiveRenditionUnittest, TestNonRealTimePlaybackRate) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // Any rate not 0.0 or 1.0 should error.
  EXPECT_CALL(*mock_mdeh_, OnError(_));
  rendition->CheckState(base::Seconds(0), 2.0, BindCheckStateNoExpect());
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestCreateRenditionPaused) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_));
  // The first segment will be queried
  RespondToUrl("http://example.com/playlist_4500Kb_14551245.ts", "tscontent");
  // Then appended.
  EXPECT_CALL(*mock_mdeh_, AppendAndParseData(_, _, _, _, _, 9))
      .WillOnce(Return(true));
  // CheckState should in this case respond with a delay of zero seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestPausedRenditionHasSomeData) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 8)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(8));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(loaded_ranges));
  // The next unqueried segment will be queried
  RespondToUrl("http://example.com/playlist_4500Kb_14551245.ts", "tscontent");
  // Then appended.
  EXPECT_CALL(*mock_mdeh_, AppendAndParseData(_, _, _, _, _, 9))
      .WillOnce(Return(true));
  // CheckState should in this case respond with a delay of zero seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestPausedRenditionHasEnoughBufferedData) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 12)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(12));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(loaded_ranges));
  // Old data will try to be removed. Since media time is 0, there is nothing
  // to do. Then there will be an attempt to fetch a new manifest, which won't
  // have any work to do either, instead just posting the delay_cb back.
  // CheckState should in this case respond with a delay of 10 / 1.5 seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(10.0 / 1.5)));
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestRenditionHasEnoughDataFetchNewManifest) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 12)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(12));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(loaded_ranges));
  // Old data will try to be removed. Since media time is 0, there is nothing
  // to do. Then there will be an attempt to fetch a new manifest, which will
  // get an update.
  task_environment_.FastForwardBy(base::Seconds(23));
  RespondToUrl("http://example.com", kSecondFetchLivePlaylist, false);

  EXPECT_CALL(*mock_hrh_, ParseMediaPlaylistFromStringSource(_, _, _))
      .WillOnce([](base::StringPiece source, GURL uri,
                   hls::types::DecimalInteger version) {
        return hls::MediaPlaylist::Parse(source, uri, version, nullptr);
      });

  // CheckState should in this case respond with a delay of 10 / 1.5 seconds.
  rendition->CheckState(base::Seconds(0), 0.0,
                        BindCheckState(base::Seconds(10.0 / 1.5)));
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestRenditionHasEnoughDataDeleteOldContent) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  // CheckState causes the rentidion to:
  // Check buffered ranges first. In this case, we've loaded a bunch of content
  // already, and our loaded ranges are [0 - 32)
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(32));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(loaded_ranges));
  // Old data will try to be removed. Since media time is 15, there are 10
  // seconds of old data to delete. There will be no new fetch and parse for
  // manifest updates.
  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(10)));
  task_environment_.FastForwardBy(base::Seconds(15));

  // CheckState should in this case respond with a delay of 10 / 1.5 seconds.
  rendition->CheckState(base::Seconds(15), 0.0,
                        BindCheckState(base::Seconds(10.0 / 1.5)));
  task_environment_.RunUntilIdle();

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestPauseAndUnpause) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);
  ASSERT_EQ(rendition->GetDuration(), absl::nullopt);

  ON_CALL(*mock_mdeh_, OnError(_)).WillByDefault([](PipelineStatus st) {
    LOG(ERROR) << MediaSerialize(st);
  });

  // Load a bunch of data, check state, will set `has_ever_played_`
  Ranges<base::TimeDelta> loaded_ranges;
  loaded_ranges.Add(base::Seconds(0), base::Seconds(32));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .Times(2)
      .WillRepeatedly(Return(loaded_ranges));
  rendition->CheckState(base::Seconds(4), 1.0,
                        BindCheckState(base::Seconds(10 / 1.5)));
  task_environment_.RunUntilIdle();

  // The pause should remove everything.
  EXPECT_CALL(*mock_mdeh_, Remove(_, base::Seconds(0), base::Seconds(32)));
  rendition->CheckState(base::Seconds(4), 0.0, BindCheckState(kNoTimestamp));
  task_environment_.RunUntilIdle();

  // Restarting playback should requery the manifest, respond with another
  // event for 0 seconds, expecting to download more
  Ranges<base::TimeDelta> post_seek_ranges;
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillRepeatedly(Return(post_seek_ranges));
  RespondToUrl("http://example.com", kSecondFetchLivePlaylist, false);
  EXPECT_CALL(*mock_hrh_, ParseMediaPlaylistFromStringSource(_, _, _))
      .WillOnce([](base::StringPiece source, GURL uri,
                   hls::types::DecimalInteger version) {
        return hls::MediaPlaylist::Parse(source, uri, version, nullptr);
      });
  rendition->CheckState(base::Seconds(4), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // It then gets called again (since it was scheduled for zero seconds),
  // and this time tries to download data.
  RespondToUrl("http://example.com/playlist_4500Kb_14551249.ts", "tscontent");
  EXPECT_CALL(*mock_mdeh_, AppendAndParseData(_, _, _, _, _, 9))
      .WillOnce(Return(true));
  rendition->CheckState(base::Seconds(4), 1.0,
                        BindCheckState(base::Seconds(0)));
  task_environment_.RunUntilIdle();

  // Loading that content creates a buffered range somewhere in the future,
  // which we then get a request to seek to.
  post_seek_ranges.Add(base::Seconds(1000), base::Seconds(1032));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillRepeatedly(Return(post_seek_ranges));
  EXPECT_CALL(*mock_mdeh_, RequestSeek(base::Seconds(1000)));
  rendition->CheckState(base::Seconds(4), 1.0, BindCheckState(kNoTimestamp));

  // From destructor.
  EXPECT_CALL(*mock_mdeh_, RemoveRole(_));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsLiveRenditionUnittest, TestStop) {
  auto rendition =
      MakeLiveRendition(GURL("http://example.com"), kInitialFetchLivePlaylist);
  ASSERT_NE(rendition, nullptr);

  rendition->Stop();

  // Should always be kNoTimestamp after `Stop()` and no network requests.
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
}

}  // namespace media
