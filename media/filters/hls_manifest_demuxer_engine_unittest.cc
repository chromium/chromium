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

const std::string kInvalidMediaPlaylist =
    "#This Wont Parse!\n"
    "#EXT-X-ENDLIST\n";

const std::string kShortMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/first.ts\n"
    "#EXT-X-ENDLIST\n";

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

const std::string kSimpleLiveMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-MEDIA-SEQUENCE:18698597\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/first.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/second.ts\n"
    "#EXTINF:3.003,\n"
    "http://media.example.com/third.ts\n";

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
using ::base::test::RunOnceClosure;
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
using testing::StrictMock;

MATCHER_P2(CloseTo,
           Target,
           Radius,
           std::string(negation ? "isn't" : "is") + " within " +
               testing::PrintToString(Radius) + " of " +
               testing::PrintToString(Target)) {
  return (arg - Target <= Radius) || (Target - arg <= Radius);
}


class FakeHlsDataSourceProvider : public HlsDataSourceProvider {
 private:
  raw_ptr<HlsDataSourceProvider> mock_;

 public:
  FakeHlsDataSourceProvider(HlsDataSourceProvider* mock) : mock_(mock) {}

  void ReadFromUrl(GURL url,
                   absl::optional<hls::types::ByteRange> range,
                   HlsDataSourceProvider::ReadCb request) override {
    mock_->ReadFromUrl(url, range, std::move(request));
  }

  void ReadFromExistingStream(std::unique_ptr<HlsDataSourceStream> stream,
                              HlsDataSourceProvider::ReadCb cb) override {
    CHECK(!stream->CanReadMore());
    std::move(cb).Run(std::move(stream));
  }

  void AbortPendingReads(base::OnceClosure callback) override {
    mock_->AbortPendingReads(std::move(callback));
  }
};

class HlsManifestDemuxerEngineTest : public testing::Test {
 protected:
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsDataSourceProvider> mock_dsp_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HlsManifestDemuxerEngine> engine_;
  std::unique_ptr<MockCodecDetector> mock_detector_;

  MOCK_METHOD(void, MockInitComplete, (PipelineStatus status), ());

  template <typename T>
  void BindUrlToDataSource(std::string url, std::string value) {
    EXPECT_CALL(*mock_dsp_, ReadFromUrl(GURL(url), _, _))
        .Times(1)
        .WillOnce(RunOnceCallback<2>(T::CreateStream(value)));
  }

 public:
  HlsManifestDemuxerEngineTest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        mock_mdeh_(std::make_unique<NiceMock<MockManifestDemuxerEngineHost>>()),
        mock_dsp_(std::make_unique<StrictMock<MockHlsDataSourceProvider>>()) {
    ON_CALL(*mock_mdeh_, AddRole(_, _, _)).WillByDefault(Return(true));
    ON_CALL(*mock_mdeh_, GetBufferedRanges(_))
        .WillByDefault(Return(Ranges<base::TimeDelta>()));

    EXPECT_CALL(*mock_dsp_, ReadFromExistingStream(_, _)).Times(0);

    base::SequenceBound<FakeHlsDataSourceProvider> dsp(
        task_environment_.GetMainThreadTaskRunner(), mock_dsp_.get());

    engine_ = std::make_unique<HlsManifestDemuxerEngine>(
        std::move(dsp), base::SingleThreadTaskRunner::GetCurrentDefault(),
        GURL("http://media.example.com/manifest.m3u8"), media_log_.get());

    mock_detector_ = std::make_unique<StrictMock<MockCodecDetector>>();
  }

  void InitializeEngine() {
    engine_->Initialize(
        mock_mdeh_.get(),
        base::BindOnce(&HlsManifestDemuxerEngineTest::MockInitComplete,
                       base::Unretained(this)));
  }

  void InitializeEngineWithMockDetector() {
    engine_->InitializeWithMockCodecDetectorForTesting(
        mock_mdeh_.get(),
        base::BindOnce(&HlsManifestDemuxerEngineTest::MockInitComplete,
                       base::Unretained(this)),
        std::move(mock_detector_));
  }

  ~HlsManifestDemuxerEngineTest() override {
    engine_->Stop();
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(HlsManifestDemuxerEngineTest, TestInitFailure) {
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kInvalidMediaPlaylist);
  EXPECT_CALL(*mock_mdeh_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_PARSE)));
  EXPECT_CALL(*this, MockInitComplete(_)).Times(0);
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(engine_->IsSeekable());
}

TEST_F(HlsManifestDemuxerEngineTest, TestSimpleConfigAddsOnePrimaryRole) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  EXPECT_CALL(*mock_mdeh_, RemoveRole(base::StringPiece("primary")));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSourceStreamFactory>(
      "http://media.example.com/first.ts", "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(engine_->IsSeekable());
}

TEST_F(HlsManifestDemuxerEngineTest, TestSimpleLiveConfigAddsOnePrimaryRole) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  EXPECT_CALL(*mock_mdeh_, RemoveRole(base::StringPiece("primary")));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleLiveMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSourceStreamFactory>(
      "http://media.example.com/first.ts", "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(engine_->IsSeekable());
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantPlaylistNoAlternates) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleMultivariantPlaylist);
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://example.com/hi.m3u8", kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSourceStreamFactory>(
      "http://media.example.com/first.ts", "bear-1280x720-hls.ts");
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
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kMultivariantPlaylistWithAlts);
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/eng-audio.m3u8", kSingleInfoMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSourceStreamFactory>(
      "http://media.example.com/only.ts", "bear-1280x720-aac_he.ts");
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/hi/video-only.m3u8", kSimpleMediaPlaylist);
  BindUrlToDataSource<FileHlsDataSourceStreamFactory>(
      "http://media.example.com/first.ts", "bear-1280x720-hls.ts");
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantWithNoSupportedCodecs) {
  EXPECT_CALL(*mock_mdeh_, AddRole(_, _, _)).Times(0);
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(_, _)).Times(0);
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kUnsupportedCodecs);
  EXPECT_CALL(*mock_mdeh_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_PARSE)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestAsyncSeek) {
  auto rendition = std::make_unique<StrictMock<MockHlsRendition>>();
  EXPECT_CALL(*rendition, GetDuration()).WillOnce(Return(base::Seconds(30)));
  auto* rendition_ptr = rendition.get();
  engine_->AddRenditionForTesting("primary", std::move(rendition));
  // Set up rendition state and run, expecting no other callbacks.
  task_environment_.RunUntilIdle();

  // When seeking, indicate that we do not need to load more buffers.
  EXPECT_CALL(*rendition_ptr, StartWaitingForSeek());
  engine_->StartWaitingForSeek();
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*rendition_ptr, Seek(_))
      .WillOnce(Return(ManifestDemuxer::SeekState::kIsReady));
  EXPECT_CALL(*mock_dsp_, AbortPendingReads(_)).WillOnce(RunOnceClosure<0>());
  engine_->Seek(base::Seconds(10),
                base::BindOnce([](ManifestDemuxer::SeekResponse resp) {
                  ASSERT_TRUE(resp.has_value());
                  ASSERT_EQ(std::move(resp).value(),
                            ManifestDemuxer::SeekState::kIsReady);
                }));
  task_environment_.RunUntilIdle();

  // Destruction should call stop.
  EXPECT_CALL(*rendition_ptr, Stop());
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultiRenditionCheckState) {
  auto rendition1 = std::make_unique<MockHlsRendition>();
  auto rendition2 = std::make_unique<MockHlsRendition>();
  EXPECT_CALL(*rendition1, GetDuration()).WillOnce(Return(absl::nullopt));
  EXPECT_CALL(*rendition2, GetDuration()).WillOnce(Return(absl::nullopt));

  auto* rend1 = rendition1.get();
  auto* rend2 = rendition2.get();
  engine_->AddRenditionForTesting("primary", std::move(rendition1));

  // While there is only one rendition, the response from |OnTimeUpdate| is
  // whatever that rendition wants.
  EXPECT_CALL(*rend1, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(7)));
  engine_->OnTimeUpdate(base::Seconds(0), 0.0,
                        base::BindOnce([](base::TimeDelta r) {
                          ASSERT_EQ(r, base::Seconds(7));
                        }));

  EXPECT_CALL(*rend1, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(kNoTimestamp));
  engine_->OnTimeUpdate(
      base::Seconds(0), 0.0,
      base::BindOnce([](base::TimeDelta r) { ASSERT_EQ(r, kNoTimestamp); }));

  // After adding the second rendition, the response from OnTimeUpdate is now
  // the lesser of (rend1.response - (calc time of rend2)) and
  // (rend2.response)
  engine_->AddRenditionForTesting("audio-override", std::move(rendition2));

  // Both renditions request time, so pick the lesser.
  EXPECT_CALL(*rend1, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(7)));
  EXPECT_CALL(*rend2, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(3)));
  engine_->OnTimeUpdate(
      base::Seconds(0), 0.0, base::BindOnce([](base::TimeDelta r) {
        EXPECT_THAT(r, CloseTo(base::Seconds(3), base::Milliseconds(1)));
      }));

  // When one rendition provides kNoTimestamp and another does not, use the
  // non-kNoTimestamp value.
  EXPECT_CALL(*rend1, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(kNoTimestamp));
  EXPECT_CALL(*rend2, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(3)));
  engine_->OnTimeUpdate(
      base::Seconds(0), 0.0, base::BindOnce([](base::TimeDelta r) {
        EXPECT_THAT(r, CloseTo(base::Seconds(3), base::Milliseconds(1)));
      }));

  EXPECT_CALL(*rend1, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(7)));
  EXPECT_CALL(*rend2, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(kNoTimestamp));
  engine_->OnTimeUpdate(
      base::Seconds(0), 0.0, base::BindOnce([](base::TimeDelta r) {
        EXPECT_THAT(r, CloseTo(base::Seconds(7), base::Milliseconds(1)));
      }));
}

TEST_F(HlsManifestDemuxerEngineTest, SeekAfterErrorFails) {
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kInvalidMediaPlaylist);
  EXPECT_CALL(*mock_mdeh_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_PARSE)));
  EXPECT_CALL(*this, MockInitComplete(_)).Times(0);
  InitializeEngine();
  task_environment_.RunUntilIdle();

  // When one of the renditions surfaces an error, ManifestDemuxer will request
  // that the engine stop. Mimic that here.
  engine_->Stop();
  task_environment_.RunUntilIdle();

  // Now if we try to seek, the response should be an instant aborted error.
  engine_->Seek(base::Seconds(10),
                base::BindOnce([](ManifestDemuxer::SeekResponse resp) {
                  ASSERT_FALSE(resp.has_value());
                  ASSERT_EQ(std::move(resp).error(), PIPELINE_ERROR_ABORT);
                }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestEndOfStreamAfterAllFetched) {
  // All the expectations set during the initialization process.
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode(base::StringPiece("primary"), true));
  EXPECT_CALL(*mock_mdeh_, AddRole(base::StringPiece("primary"), "video/mp2t",
                                   "avc1.420000, mp4a.40.05"));
  EXPECT_CALL(*mock_mdeh_, SetDuration(9.009));
  HlsCodecDetector::ContainerAndCodecs mock_response = {
      "video/mp2t", "avc1.420000, mp4a.40.05"};
  EXPECT_CALL(*mock_detector_, DetermineContainerAndCodec(_, _))
      .WillOnce(RunOnceCallback<1>(mock_response));
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));

  // We can't use `BindUrlToDataSource` here, since it can't re-create streams
  // like we need it to. The network requests are in order:
  // - manifest.m3u8 - main manifest
  // - first.ts      - request for the first few bytes to do codec detection
  // - first.ts      - request for chunks of data to add to ChunkDemuxer
  EXPECT_CALL(*mock_dsp_,
              ReadFromUrl(GURL("http://media.example.com/manifest.m3u8"), _, _))
      .WillOnce(RunOnceCallback<2>(
          StringHlsDataSourceStreamFactory::CreateStream(kShortMediaPlaylist)));
  EXPECT_CALL(*mock_dsp_,
              ReadFromUrl(GURL("http://media.example.com/first.ts"), _, _))
      .WillOnce(
          RunOnceCallback<2>(StringHlsDataSourceStreamFactory::CreateStream(
              "hey, this isn't a bitstream!")))
      .WillOnce(
          RunOnceCallback<2>(StringHlsDataSourceStreamFactory::CreateStream(
              "do I look like a video to you?")));

  // `GetBufferedRanges` gets called many times during this process:
  // - HlsVodRendition::CheckState (1) => empty ranges, nothing loaded.
  // - HlsVodRendition::OnSegmentData (1) => populated by AppendAndParseData
  // - HlsVodRendition::CheckState (2) => still has data
  Ranges<base::TimeDelta> populated_ranges;
  populated_ranges.Add(base::Seconds(0), base::Seconds(5));
  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(Ranges<base::TimeDelta>()))
      .WillOnce(Return(populated_ranges))
      .WillOnce(Return(populated_ranges));

  // The first call to `OnTimeUpdate` should trigger the append function,
  // and our data was 30 characters long.
  EXPECT_CALL(*mock_mdeh_,
              AppendAndParseData("primary", base::Seconds(0), _, _, _, 30))
      .WillOnce(Return(true));

  // Finally, and EndOfStream call happens:
  EXPECT_CALL(*mock_mdeh_, SetEndOfStream());

  // And then teardown:
  EXPECT_CALL(*mock_mdeh_, RemoveRole(base::StringPiece("primary")));

  // Setup with a mock codec detector - this will set all the roles, duration,
  // modes, and also make a request for the manifest and the first segment.
  InitializeEngineWithMockDetector();
  task_environment_.RunUntilIdle();

  // For the first state check, there should be empty ranges, which triggers
  // `HlsVodRendition::FetchNext`, which should request the data from first.ts
  // add its content, and then return.
  engine_->OnTimeUpdate(base::Seconds(0), 1.0, base::DoNothing());
  task_environment_.RunUntilIdle();

  // For the second state check, there are no more segments, no pending segment,
  // and there are loaded ranges, so HlsVodRendition will report an EndOfStream.
  engine_->OnTimeUpdate(base::Seconds(6), 1.0, base::DoNothing());
  task_environment_.RunUntilIdle();

  // Expectations on teardown.
  ASSERT_TRUE(engine_->IsSeekable());
  task_environment_.RunUntilIdle();
}

}  // namespace media
