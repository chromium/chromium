// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_manifest_demuxer_engine.h"
#include "media/filters/manifest_demuxer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const std::string kSimpleMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/first.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/second.ts\n"
    "#EXTINF:3.003,\n"
    "http://media.example.com/third.ts\n"
    "#EXT-X-ENDLIST\n";

const std::string kSingleInfoMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/only.ts\n";

const std::string kUnsupportedCodecs =
    "#EXTM3U\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"vvc1.00.00\"\n"
    "http://example.com/audio-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"sheet.music\"\n"
    "http://example.com/audio-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"av02.00.00\"\n"
    "http://example.com/audio-only.m3u8\n";

const std::string kSimpleMultivariantPlaylist =
    "#EXTM3U\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000\n"
    "http://example.com/low.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=2560000,AVERAGE-BANDWIDTH=2000000\n"
    "http://example.com/mid.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=7680000,AVERAGE-BANDWIDTH=6000000\n"
    "http://example.com/hi.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n"
    "http://example.com/audio-only.m3u8\n";

const std::string kMultivariantPlaylistWithAlts =
    "#EXTM3U\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Eng\",DEFAULT=YES,"
    "AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"eng-audio.m3u8\"\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Ger\",DEFAULT=NO,"
    "AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"ger-audio.m3u8\"\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Com\",DEFAULT=NO,"
    "AUTOSELECT=NO,LANGUAGE=\"en\",URI=\"eng-comments.m3u8\"\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=1280000,CODECS=\"avc1.420000\",AUDIO=\"aac\"\n"
    "low/video-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=2560000,CODECS=\"avc1.420000\",AUDIO=\"aac\"\n"
    "mid/video-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=7680000,CODECS=\"avc1.420000\",AUDIO=\"aac\"\n"
    "hi/video-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.05\",AUDIO=\"aac\"\n"
    "main/english-audio.m3u8\n";

using ::base::test::RunOnceCallback;
using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

class MockManifestDemuxerEngineHost : public ManifestDemuxerEngineHost {
 public:
  MOCK_METHOD(bool,
              AddRole,
              (base::StringPiece, std::string, std::string),
              (override));
  MOCK_METHOD(void, RemoveRole, (base::StringPiece), (override));
  MOCK_METHOD(void, SetSequenceMode, (base::StringPiece, bool), (override));
  MOCK_METHOD(void, SetDuration, (double), (override));
  MOCK_METHOD(Ranges<base::TimeDelta>,
              GetBufferedRanges,
              (base::StringPiece),
              (override));
  MOCK_METHOD(void,
              Remove,
              (base::StringPiece, base::TimeDelta, base::TimeDelta),
              (override));
  MOCK_METHOD(
      void,
      RemoveAndReset,
      (base::StringPiece, base::TimeDelta, base::TimeDelta, base::TimeDelta*),
      (override));
  MOCK_METHOD(void,
              SetGroupStartIfParsingAndSequenceMode,
              (base::StringPiece, base::TimeDelta),
              (override));
  MOCK_METHOD(void,
              EvictCodedFrames,
              (base::StringPiece, base::TimeDelta, size_t),
              (override));
  MOCK_METHOD(bool,
              AppendAndParseData,
              (base::StringPiece,
               base::TimeDelta,
               base::TimeDelta,
               base::TimeDelta*,
               const uint8_t*,
               size_t),
              (override));
  MOCK_METHOD(void, OnError, (PipelineStatus), (override));
};

class MockHlsDataSourceProvider : public HlsDataSourceProvider {
 public:
  MOCK_METHOD(std::unique_ptr<HlsDataSource>, GetDataSource, (std::string));
  void RequestDataSource(GURL url,
                         absl::optional<hls::types::ByteRange> br,
                         RequestCb cb) override {
    std::move(cb).Run(GetDataSource(url.spec()));
  }
};

class FakeHlsDataSourceProvider : public HlsDataSourceProvider {
 private:
  base::raw_ptr<HlsDataSourceProvider> mock_;

 public:
  FakeHlsDataSourceProvider(HlsDataSourceProvider* mock) : mock_(mock) {}

  void RequestDataSource(GURL url,
                         absl::optional<hls::types::ByteRange> range,
                         RequestCb request) override {
    mock_->RequestDataSource(url, range, std::move(request));
  }
};

class HlsManifestDemuxerEngineTest : public testing::Test {
 protected:
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsDataSourceProvider> mock_dsp_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HlsManifestDemuxerEngine> engine_;

  MOCK_METHOD(void, MockInitComplete, (PipelineStatus status), ());

  template <typename T>
  void BindUrlToDataSource(std::string url, std::string value) {
    EXPECT_CALL(*mock_dsp_, GetDataSource(url))
        .Times(1)
        .WillOnce(Return(ByMove(std::make_unique<T>(value))));
  }

 public:
  HlsManifestDemuxerEngineTest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        mock_mdeh_(std::make_unique<MockManifestDemuxerEngineHost>()),
        mock_dsp_(std::make_unique<MockHlsDataSourceProvider>()) {
    ON_CALL(*mock_mdeh_, AddRole(_, _, _)).WillByDefault(Return(true));
    ON_CALL(*mock_mdeh_, GetBufferedRanges(_))
        .WillByDefault(Return(Ranges<base::TimeDelta>()));

    base::SequenceBound<FakeHlsDataSourceProvider> dsp(
        task_environment_.GetMainThreadTaskRunner(), mock_dsp_.get());

    engine_ = std::make_unique<HlsManifestDemuxerEngine>(
        std::move(dsp), base::SingleThreadTaskRunner::GetCurrentDefault(),
        GURL("http://media.example.com/manifest.m3u8"), media_log_.get());
  }

  void InitializeEngine() {
    engine_->Initialize(
        mock_mdeh_.get(),
        base::BindOnce(&HlsManifestDemuxerEngineTest::MockInitComplete,
                       base::Unretained(this)));
  }

  ~HlsManifestDemuxerEngineTest() override { base::RunLoop().RunUntilIdle(); }
};

TEST_F(HlsManifestDemuxerEngineTest, TestSimpleConfigAddsOnePrimaryRole) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/manifest.m3u8", kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSource>("http://media.example.com/first.ts",
                                         "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantPlaylistNoAlternates) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/manifest.m3u8", kSimpleMultivariantPlaylist);
  BindUrlToDataSource<StringHlsDataSource>("http://example.com/hi.m3u8",
                                           kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSource>("http://media.example.com/first.ts",
                                         "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantPlaylistWithAlternates) {
  EXPECT_CALL(*mock_mdeh_,
              SetSequenceMode(base::StringPiece("audio-override"), true));
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("audio-override"),
                                   "video/mp2t", "avc1.420000"));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000"));

  // URL queries in order:
  //  - manifest.m3u8: root manifest
  //  - eng-audio.m3u8: audio override rendition playlist
  //  - only.ts: check the container/codecs for the audio override rendition
  //  - video-only.m3u8: primary rendition
  //  - first.ts: check container/codecs for the primary rendition
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/manifest.m3u8", kMultivariantPlaylistWithAlts);
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/eng-audio.m3u8", kSingleInfoMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSource>("http://media.example.com/only.ts",
                                         "bear-1280x720-aac_he.ts");
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/hi/video-only.m3u8", kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSource>("http://media.example.com/first.ts",
                                         "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantWithNoSupportedCodecs) {
  EXPECT_CALL(*mock_mdeh_, AddRole(_, _, _)).Times(0);
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(_, _)).Times(0);
  BindUrlToDataSource<StringHlsDataSource>(
      "http://media.example.com/manifest.m3u8", kUnsupportedCodecs);
  EXPECT_CALL(*mock_mdeh_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_PARSE)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

}  // namespace media
