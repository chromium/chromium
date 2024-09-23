// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_media_player_tag_recorder.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "media/filters/hls_network_access.h"
#include "media/filters/hls_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
using ::base::test::RunOnceCallback;
using testing::_;
using testing::StrictMock;

const std::string kURL = "https://example.com/example.m3u8";

const std::string kEncryptedSimpleStream =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-PLAYLIST-TYPE:VOD\n"
    "#EXT-X-INDEPENDENT-SEGMENTS\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"/"
    "enc.key\",IV=0xf4d52cf0dc02329c3ad6578744590658\n"
    "#EXTINF:1.600001,\n"
    "data00.ts\n"
    "#EXTINF:1.600002,\n"
    "data01.ts\n"
    "#EXTINF:1.600003,\n"
    "data02.ts\n"
    "#EXTINF:1.600004,\n"
    "data03.ts\n"
    "#EXTINF:1.600000,\n"
    "data04.ts\n"
    "#EXTINF:1.600000,\n"
    "data05.ts\n"
    "#EXTINF:1.600000,\n"
    "data06.ts\n"
    "#EXTINF:0.066667,\n"
    "data07.ts\n"
    "#EXT-X-ENDLIST\n";

const std::string kEncryptedSimpleStreamBadIV =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXT-X-PLAYLIST-TYPE:VOD\n"
    "#EXT-X-INDEPENDENT-SEGMENTS\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"/"
    "enc.key\",IV=0xf4d52cf0dc02329c3ad578744590658\n"
    "#EXTINF:1.600001,\n"
    "data00.ts\n"
    "#EXTINF:1.600002,\n"
    "data01.ts\n"
    "#EXTINF:1.600003,\n"
    "data02.ts\n"
    "#EXTINF:1.600004,\n"
    "data03.ts\n"
    "#EXTINF:1.600000,\n"
    "data04.ts\n"
    "#EXTINF:1.600000,\n"
    "data05.ts\n"
    "#EXTINF:1.600000,\n"
    "data06.ts\n"
    "#EXTINF:0.066667,\n"
    "data07.ts\n"
    "#EXT-X-ENDLIST\n";

class HlsMediaPlayerTagRecorderTest : public testing::Test {
 public:
  HlsMediaPlayerTagRecorderTest() {
    auto network_access = std::make_unique<StrictMock<MockHlsNetworkAccess>>();
    network = network_access.get();
    recorder =
        std::make_unique<HlsMediaPlayerTagRecorder>(std::move(network_access));
  }

  ~HlsMediaPlayerTagRecorderTest() override { network = nullptr; }

  void BindManifest(std::string url,
                    std::string value,
                    bool taint_origin = false) {
    EXPECT_CALL(*network, ReadManifest(GURL(url), _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(
            StringHlsDataSourceStreamFactory::CreateStream(value, false)));
  }

 protected:
  raw_ptr<MockHlsNetworkAccess> network;
  std::unique_ptr<HlsMediaPlayerTagRecorder> recorder;
};

#define EXPECT_UMA(HT, NAME, VALUE)        \
  do {                                     \
    HT.ExpectUniqueSample(NAME, VALUE, 1); \
  } while (0)

#define EXPECT_NO_UMA(HT, NAME, VALUE)     \
  do {                                     \
    HT.ExpectUniqueSample(NAME, VALUE, 0); \
  } while (0)

TEST_F(HlsMediaPlayerTagRecorderTest, TestReadManifestParseBeforeOkSignal) {
  base::HistogramTester ht;
  BindManifest(kURL, kEncryptedSimpleStream);

  recorder->Start(GURL(kURL));

  EXPECT_NO_UMA(ht, "Media.HLS.PlaylistSegmentExtension", 0);
  EXPECT_NO_UMA(ht, "Media.HLS.AdvancedFeatureTags", 4);
  EXPECT_NO_UMA(ht, "Media.HLS.MultivariantPlaylist", 0);
  EXPECT_NO_UMA(ht, "Media.HLS.EncryptionMode", 1);

  recorder->AllowRecording();

  EXPECT_UMA(ht, "Media.HLS.PlaylistSegmentExtension", 0);
  EXPECT_UMA(ht, "Media.HLS.AdvancedFeatureTags", 4);
  EXPECT_UMA(ht, "Media.HLS.MultivariantPlaylist", 0);
  EXPECT_UMA(ht, "Media.HLS.EncryptionMode", 1);
}

TEST_F(HlsMediaPlayerTagRecorderTest, TestReadManifestParseAfterOkSignal) {
  base::HistogramTester ht;
  BindManifest(kURL, kEncryptedSimpleStream);

  recorder->AllowRecording();

  EXPECT_NO_UMA(ht, "Media.HLS.PlaylistSegmentExtension", 0);
  EXPECT_NO_UMA(ht, "Media.HLS.AdvancedFeatureTags", 4);
  EXPECT_NO_UMA(ht, "Media.HLS.MultivariantPlaylist", 0);
  EXPECT_NO_UMA(ht, "Media.HLS.EncryptionMode", 1);

  recorder->Start(GURL(kURL));

  EXPECT_UMA(ht, "Media.HLS.PlaylistSegmentExtension", 0);
  EXPECT_UMA(ht, "Media.HLS.AdvancedFeatureTags", 4);
  EXPECT_UMA(ht, "Media.HLS.MultivariantPlaylist", 0);
  EXPECT_UMA(ht, "Media.HLS.EncryptionMode", 1);
}

TEST_F(HlsMediaPlayerTagRecorderTest, TestInvalidManifest) {
  base::HistogramTester ht;
  BindManifest(kURL, kEncryptedSimpleStreamBadIV);
  recorder->AllowRecording();
  recorder->Start(GURL(kURL));
  EXPECT_UMA(ht, "Media.HLS.UnparsableManifest", 2);
}

#undef EXPECT_NO_UMA
#undef EXPECT_UMA

}  // namespace media
