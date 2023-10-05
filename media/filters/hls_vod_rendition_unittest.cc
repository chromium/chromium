// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_vod_rendition.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_test_helpers.h"

namespace media {

namespace {

constexpr char kInitialFetchVodPlaylist[] =
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

}  // namespace

using testing::_;
using testing::Return;

class HlsVodRenditionUnittest : public testing::Test {
 protected:
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsRenditionHost> mock_hrh_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HlsVodRendition> MakeVodRendition(base::StringPiece content) {
    constexpr hls::types::DecimalInteger version = 3;
    auto parsed = hls::MediaPlaylist::Parse(
        content, GURL("https://example.m3u8"), version, nullptr);
    if (!parsed.has_value()) {
      LOG(ERROR) << MediaSerialize(std::move(parsed).error());
      return nullptr;
    }
    auto playlist = std::move(parsed).value();
    auto duration = playlist->GetComputedDuration();
    return std::make_unique<HlsVodRendition>(mock_mdeh_.get(), mock_hrh_.get(),
                                             "test", std::move(playlist),
                                             duration);
  }

  MOCK_METHOD(void, CheckStateComplete, (base::TimeDelta delay), ());

  ManifestDemuxer::DelayCallback BindCheckState(base::TimeDelta time) {
    EXPECT_CALL(*this, CheckStateComplete(time));
    return base::BindOnce(&HlsVodRenditionUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  ManifestDemuxer::DelayCallback BindCheckStateNoExpect() {
    return base::BindOnce(&HlsVodRenditionUnittest::CheckStateComplete,
                          base::Unretained(this));
  }

  void RespondWithRange(base::TimeDelta start, base::TimeDelta end) {
    Ranges<base::TimeDelta> ranges;
    if (start != end) {
      ranges.Add(start, end);
    }
    EXPECT_CALL(*mock_mdeh_, GetBufferedRanges("test"))
        .WillOnce(Return(ranges));
  }

  void SupplyAndExpectJunkData(base::TimeDelta initial_response_start,
                               base::TimeDelta initial_response_end,
                               base::TimeDelta fetch_expected_time) {
    std::string junk_content = "abcdefg, I dont like to sing rhyming songs";
    EXPECT_CALL(*mock_hrh_, ReadFromUrl(_, _, _, _))
        .WillOnce([content = std::move(junk_content), host = mock_hrh_.get()](
                      GURL url, bool, absl::optional<hls::types::ByteRange>,
                      HlsDataSourceProvider::ReadCb cb) {
          auto stream = StringHlsDataSourceStreamFactory::CreateStream(content);
          std::move(cb).Run(std::move(stream));
        });
    EXPECT_CALL(*mock_mdeh_, AppendAndParseData("test", _, _, _, _, 42))
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

 public:
  HlsVodRenditionUnittest()
      : mock_mdeh_(std::make_unique<MockManifestDemuxerEngineHost>()),
        mock_hrh_(std::make_unique<MockHlsRenditionHost>()) {
    EXPECT_CALL(*mock_mdeh_, RemoveRole("test"));
  }
};

TEST_F(HlsVodRenditionUnittest, TestCheckStateFromNoData) {
  auto rendition = MakeVodRendition(kInitialFetchVodPlaylist);
  ASSERT_NE(rendition, nullptr);

  SupplyAndExpectJunkData(base::Seconds(0), base::Seconds(0), base::Seconds(1));
  rendition->CheckState(base::Seconds(0), 1.0,
                        BindCheckState(base::Seconds(0)));

  task_environment_.RunUntilIdle();
}

TEST_F(HlsVodRenditionUnittest, TestCheckStateWithLargeBufferCached) {
  auto rendition = MakeVodRendition(kInitialFetchVodPlaylist);
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

TEST_F(HlsVodRenditionUnittest, TestCheckStateWithTooLateBuffer) {
  auto rendition = MakeVodRendition(kInitialFetchVodPlaylist);
  ASSERT_NE(rendition, nullptr);

  RespondWithRange(base::Seconds(10), base::Seconds(12));
  EXPECT_CALL(*mock_mdeh_, OnError(_));
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckStateNoExpect());

  task_environment_.RunUntilIdle();
}

TEST_F(HlsVodRenditionUnittest, TestStop) {
  auto rendition = MakeVodRendition(kInitialFetchVodPlaylist);
  ASSERT_NE(rendition, nullptr);

  rendition->Stop();

  // Should always be kNoTimestamp after `Stop()` and no network requests.
  rendition->CheckState(base::Seconds(0), 1.0, BindCheckState(kNoTimestamp));
}

}  // namespace media
