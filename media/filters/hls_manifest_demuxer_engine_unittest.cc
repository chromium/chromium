// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_manifest_demuxer_engine.h"

#include <memory>
#include <string>
#include <string_view>
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
#include "media/filters/manifest_demuxer.h"
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

const std::string kInitialRequestLiveMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-MEDIA-SEQUENCE:1234\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/a.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/b.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/c.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/d.ts\n";

const std::string kSecondRequestLiveMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-MEDIA-SEQUENCE:1236\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/c.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/d.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/e.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/f.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/g.ts\n"
    "#EXTINF:9.009,\n"
    "http://media.example.com/h.ts\n";

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

const std::string kLiveFullEncryptedMediaPlaylist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:4\n"
    "#EXT-X-TARGETDURATION:4\n"
    "#EXT-X-MEDIA-SEQUENCE:13979\n"
    "#EXT-X-DISCONTINUITY-SEQUENCE:0\n"
    "#EXT-X-KEY:METHOD=AES-128,URI=\"K\",IV=0x66666666666666666666666666666666,"
    "KEYFORMAT=\"identity\",KEYFORMATVERSIONS=\"1\"\n"
    "#EXTINF:3.0,\n"
    "13979.js\n"
    "#EXTINF:3.0,\n"
    "13980.js\n"
    "#EXTINF:3.0,\n"
    "13981.js\n";

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

MATCHER_P2(SingleSegmentQueue,
           urlstr,
           range,
           "Segment Queue matcher for HlsDataSourceProvider") {
  if (arg.size() != 1) {
    return false;
  }
  auto first = arg.front();
  return first.uri == GURL(urlstr) && first.range == range;
}

std::tuple<std::string, std::unique_ptr<crypto::SymmetricKey>> Encrypt(
    std::string cleartext,
    std::string ivstr) {
  std::string ciphertext;
  auto mode = crypto::SymmetricKey::AES;
  auto key = crypto::SymmetricKey::GenerateRandomKey(mode, 128);
  auto encryptor = std::make_unique<crypto::Encryptor>();
  encryptor->Init(key.get(), crypto::Encryptor::Mode::CBC, ivstr);
  encryptor->Encrypt(cleartext, &ciphertext);
  return std::make_tuple(ciphertext, std::move(key));
}

class FakeHlsDataSourceProvider : public HlsDataSourceProvider {
 private:
  raw_ptr<HlsDataSourceProvider> mock_;

 public:
  FakeHlsDataSourceProvider(HlsDataSourceProvider* mock) : mock_(mock) {}

  void ReadFromCombinedUrlQueue(
      SegmentQueue segments,
      HlsDataSourceProvider::ReadCb callback) override {
    mock_->ReadFromCombinedUrlQueue(std::move(segments), std::move(callback));
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

template <typename T>
class CallbackEnforcer {
 public:
  explicit CallbackEnforcer(T expected)
      : expected_(std::move(expected)), was_called_(false) {}

  base::OnceCallback<void(T)> GetCallback() {
    return base::BindOnce(
        [](bool* writeback, T expected, T actual) {
          *writeback = true;
          ASSERT_EQ(actual, expected);
        },
        &was_called_, expected_);
  }

  // This method is move only, so it must be std::moved.
  void AssertAndReset(base::test::TaskEnvironment& env) && {
    env.RunUntilIdle();
    ASSERT_TRUE(was_called_);
  }

 private:
  T expected_;
  bool was_called_ = false;
};

class HlsManifestDemuxerEngineTest : public testing::Test {
 protected:
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockManifestDemuxerEngineHost> mock_mdeh_;
  std::unique_ptr<MockHlsDataSourceProvider> mock_dsp_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<HlsManifestDemuxerEngine> engine_;

  base::OnceClosure pending_url_fetch_;

  template <typename T>
  void BindUrlToDataSource(std::string url,
                           std::string value,
                           bool taint_origin = false) {
    EXPECT_CALL(*mock_dsp_, ReadFromCombinedUrlQueue(
                                SingleSegmentQueue(url, std::nullopt), _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(T::CreateStream(value, taint_origin)));
  }

  template <typename T>
  void BindUrlAssignmentThunk(std::string url,
                              std::string value,
                              bool taint_origin = false) {
    EXPECT_CALL(*mock_dsp_,
                ReadFromCombinedUrlQueue(
                    SingleSegmentQueue(std::move(url), std::nullopt), _))
        .Times(1)
        .WillOnce([this, value = std::move(value), taint_origin = taint_origin](
                      HlsDataSourceProvider::SegmentQueue,
                      HlsDataSourceProvider::ReadCb cb) {
          pending_url_fetch_ = base::BindOnce(
              std::move(cb), T::CreateStream(std::move(value), taint_origin));
        });
  }

  void ExpectNoNetworkRequests() {
    EXPECT_CALL(*mock_dsp_, ReadFromCombinedUrlQueue(_, _)).Times(0);
  }

  MockHlsRendition* SetUpInterruptTest() {
    EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
    EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
    EXPECT_CALL(*mock_mdeh_,
                AddRole("primary", RelaxedParserSupportedType::kMP2T));
    BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
        "http://media.example.com/manifest.m3u8", kSimpleMultivariantPlaylist);
    BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
        "http://example.com/hi.m3u8", kSimpleMediaPlaylist);
    EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
    InitializeEngine();
    task_environment_.RunUntilIdle();

    auto rendition = std::make_unique<StrictMock<MockHlsRendition>>();
    EXPECT_CALL(*rendition, GetDuration()).WillOnce(Return(base::Seconds(30)));
    auto* rendition_ptr = rendition.get();
    engine_->AddRenditionForTesting("primary", std::move(rendition));
    EXPECT_CALL(*rendition_ptr, Stop());
    task_environment_.RunUntilIdle();

    return rendition_ptr;
  }

  base::OnceClosure StartAndCaptureSeek(MockHlsRendition* rendition_ptr) {
    base::OnceClosure continue_seek;
    EXPECT_CALL(*this, SeekFinished()).Times(0);
    EXPECT_CALL(*rendition_ptr, Seek(_)).Times(0);
    EXPECT_CALL(*mock_dsp_, AbortPendingReads(_))
        .WillOnce([&continue_seek](base::OnceClosure cb) {
          continue_seek = std::move(cb);
        });

    engine_->Seek(
        base::Seconds(10),
        base::BindOnce(
            [](base::OnceClosure cb, ManifestDemuxer::SeekResponse resp) {
              ASSERT_TRUE(resp.has_value());
              std::move(cb).Run();
            },
            base::BindOnce(&HlsManifestDemuxerEngineTest::SeekFinished,
                           base::Unretained(this))));
    task_environment_.RunUntilIdle();
    CHECK(continue_seek);
    return base::BindOnce(
        [](MockHlsRendition* rendition_ptr, base::OnceClosure cb) {
          EXPECT_CALL(*rendition_ptr, Seek(_))
              .WillOnce(Return(ManifestDemuxer::SeekState::kIsReady));
          std::move(cb).Run();
        },
        rendition_ptr, std::move(continue_seek));
  }

  base::OnceClosure StartAndCaptureTimeUpdate(MockHlsRendition* rendition_ptr,
                                              base::TimeDelta timestamp) {
    ManifestDemuxer::DelayCallback finish_time_update;
    EXPECT_CALL(*rendition_ptr, CheckState(_, _, _))
        .WillOnce([&finish_time_update](base::TimeDelta, double,
                                        ManifestDemuxer::DelayCallback cb) {
          finish_time_update = std::move(cb);
        });
    engine_->OnTimeUpdate(
        base::Seconds(0), 0.0,
        base::BindOnce([](base::TimeDelta timestamp,
                          base::TimeDelta r) { ASSERT_EQ(r, timestamp); },
                       timestamp));
    task_environment_.RunUntilIdle();
    CHECK(finish_time_update);
    return base::BindOnce(std::move(finish_time_update), timestamp);
  }

  base::OnceClosure StartAndCaptureNetworkAdaptation(
      MockHlsRendition* rendition_ptr,
      std::string url,
      std::string value,
      size_t netspeed) {
    base::OnceClosure continue_adaptation;
    EXPECT_CALL(*mock_dsp_, ReadFromCombinedUrlQueue(
                                SingleSegmentQueue(url, std::nullopt), _))
        .Times(1)
        .WillOnce(
            [&continue_adaptation, value](HlsDataSourceProvider::SegmentQueue,
                                          HlsDataSourceProvider::ReadCb cb) {
              continue_adaptation = base::BindOnce(
                  std::move(cb),
                  StringHlsDataSourceStreamFactory::CreateStream(value));
            });
    engine_->UpdateNetworkSpeed(netspeed);
    task_environment_.RunUntilIdle();
    CHECK(continue_adaptation);
    return base::BindOnce(
        [](MockHlsRendition* rendition_ptr, base::OnceClosure cb) {
          EXPECT_CALL(*rendition_ptr, UpdatePlaylist(_, _));
          std::move(cb).Run();
        },
        rendition_ptr, std::move(continue_adaptation));
  }

 public:
  MOCK_METHOD(void, MockInitComplete, (PipelineStatus status), ());
  MOCK_METHOD(void, SeekFinished, (), ());
  MOCK_METHOD(void, AddMediaTrack, (const MediaTrack&), ());
  MOCK_METHOD(void, RemoveMediaTrack, (const MediaTrack&), ());

  HlsManifestDemuxerEngineTest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        mock_mdeh_(std::make_unique<NiceMock<MockManifestDemuxerEngineHost>>()),
        mock_dsp_(std::make_unique<StrictMock<MockHlsDataSourceProvider>>()) {
    ON_CALL(*mock_mdeh_, AddRole(_, _)).WillByDefault(Return(true));
    ON_CALL(*mock_mdeh_, GetBufferedRanges(_))
        .WillByDefault(Return(Ranges<base::TimeDelta>()));

    EXPECT_CALL(*mock_dsp_, ReadFromExistingStream(_, _)).Times(0);

    base::SequenceBound<FakeHlsDataSourceProvider> dsp(
        task_environment_.GetMainThreadTaskRunner(), mock_dsp_.get());

    engine_ = std::make_unique<HlsManifestDemuxerEngine>(
        std::move(dsp), base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindRepeating(&HlsManifestDemuxerEngineTest::AddMediaTrack,
                            base::Unretained(this)),
        base::BindRepeating(&HlsManifestDemuxerEngineTest::RemoveMediaTrack,
                            base::Unretained(this)),
        false, GURL("http://media.example.com/manifest.m3u8"),
        media_log_.get());
  }

  void InitializeEngine() {
    engine_->Initialize(
        mock_mdeh_.get(),
        base::BindOnce(&HlsManifestDemuxerEngineTest::MockInitComplete,
                       base::Unretained(this)));
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
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  EXPECT_CALL(*mock_mdeh_, RemoveRole("primary"));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(engine_->IsSeekable());
}

TEST_F(HlsManifestDemuxerEngineTest, TestSimpleLiveConfigAddsOnePrimaryRole) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  EXPECT_CALL(*mock_mdeh_, RemoveRole("primary"));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleLiveMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(engine_->IsSeekable());
}

TEST_F(HlsManifestDemuxerEngineTest, TestLivePlaybackManifestUpdates) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8",
      kInitialRequestLiveMediaPlaylist);

  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();

  // Assume that anything appended is valid, because we actually have no valid
  // media for this test.
  EXPECT_CALL(*mock_mdeh_, AppendAndParseData("primary", _, _, _))
      .WillRepeatedly(Return(true));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/a.ts", "Cheese in a cstring is string cheese.");
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/b.ts",
      "Tomatoes are a fruit. Ketchup is a jam.");
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/c.ts", "You've never been in an empty room.");

  Ranges<base::TimeDelta> after_seg_a;
  after_seg_a.Add(base::Seconds(0), base::Seconds(9));

  Ranges<base::TimeDelta> after_seg_b;
  after_seg_b.Add(base::Seconds(0), base::Seconds(18));

  Ranges<base::TimeDelta> after_seg_c;
  after_seg_c.Add(base::Seconds(0), base::Seconds(27));

  EXPECT_CALL(*mock_mdeh_, GetBufferedRanges(_))
      .WillOnce(Return(Ranges<base::TimeDelta>()))  // First CheckState
      .WillOnce(Return(after_seg_a))                // After appending segment A
      .WillOnce(Return(after_seg_a))                // Second CheckState
      .WillOnce(Return(after_seg_b))                // After appending segment B
      .WillOnce(Return(after_seg_b))                // Third CheckState
      .WillOnce(Return(after_seg_b))                // Fourth CheckState
      .WillOnce(Return(after_seg_c))                // After appending segment C
      .WillOnce(Return(after_seg_c))                // Fifth CheckState
      ;

  // Give a playback-rate=1 time update, signaling start of play. Since a data
  // append happens, we should ask for a delay of 0.
  CallbackEnforcer<base::TimeDelta> first(base::Seconds(0));
  engine_->OnTimeUpdate(base::Seconds(0), 1.0, first.GetCallback());
  std::move(first).AssertAndReset(task_environment_);

  // Make another request. This time, we have some data, but it's less than the
  // ideal buffer size, so it will make another request to append data. Again
  // because there was an append, we ask for a delay of 0.
  CallbackEnforcer<base::TimeDelta> second(base::Seconds(0));
  engine_->OnTimeUpdate(base::Seconds(0), 1.0, second.GetCallback());
  std::move(second).AssertAndReset(task_environment_);

  // Lets pretend time ticked forward 1 second while we were making that network
  // request for segment 2, and now the range is {0-18}. we calculate that we
  // can delay for (buffer - ideal/2) => (17 - 10/2) => 12.
  CallbackEnforcer<base::TimeDelta> third(base::Seconds(12));
  task_environment_.FastForwardBy(base::Seconds(1));
  engine_->OnTimeUpdate(base::Seconds(1), 1.0, third.GetCallback());
  std::move(third).AssertAndReset(task_environment_);

  // Lets fast forward those 12 seconds and make another request. This will
  // leave a remaining buffer of 5 seconds, which will trigger another segment
  // read, which means a delay of 0.
  CallbackEnforcer<base::TimeDelta> fourth(base::Seconds(0));
  task_environment_.FastForwardBy(base::Seconds(12));
  engine_->OnTimeUpdate(base::Seconds(13), 1.0, fourth.GetCallback());
  std::move(fourth).AssertAndReset(task_environment_);

  // This time, The buffer is large enough - 27 seconds is the end while the
  // media time is only 13 seconds, but we've popped 3/4 segments from the
  // queue, which leaves 1. Multiplying that by the 10 second max duration, we
  // find that because it's been 13 seconds since the last manifest update, its
  // time to make another one. We bind a new data stream to the manifest URL,
  // which should populate the queue with more segments. There should then
  // be a response of (27 - 13) - (10/2), or 9 seconds.
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8",
      kSecondRequestLiveMediaPlaylist);
  CallbackEnforcer<base::TimeDelta> fifth(base::Seconds(9));
  engine_->OnTimeUpdate(base::Seconds(13), 1.0, fifth.GetCallback());
  std::move(fifth).AssertAndReset(task_environment_);
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantPlaylistNoAlternates) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleMultivariantPlaylist);
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://example.com/hi.m3u8", kSimpleMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantPlaylistWithAlternates) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("audio-override", true));
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("audio-override", RelaxedParserSupportedType::kMP2T));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));

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
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/hi/video-only.m3u8", kSimpleMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestMultivariantWithNoSupportedCodecs) {
  EXPECT_CALL(*mock_mdeh_, AddRole(_, _)).Times(0);
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
  EXPECT_CALL(*rendition1, GetDuration()).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*rendition2, GetDuration()).WillOnce(Return(std::nullopt));

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

TEST_F(HlsManifestDemuxerEngineTest, TestSeekDuringAdaptation) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the adaptation and hold it from finishing.
  base::OnceClosure continue_adaptation = StartAndCaptureNetworkAdaptation(
      rendition_ptr, "http://example.com/low.m3u8", kSimpleMediaPlaylist,
      1380000);

  // Start a seek. It should wait while the adaptation is pending.
  EXPECT_CALL(*this, SeekFinished()).Times(0);
  EXPECT_CALL(*rendition_ptr, Seek(_)).Times(0);
  EXPECT_CALL(*mock_dsp_, AbortPendingReads(_)).Times(0);
  engine_->Seek(
      base::Seconds(10),
      base::BindOnce(
          [](base::OnceClosure cb, ManifestDemuxer::SeekResponse resp) {
            ASSERT_TRUE(resp.has_value());
            std::move(cb).Run();
          },
          base::BindOnce(&HlsManifestDemuxerEngineTest::SeekFinished,
                         base::Unretained(this))));
  task_environment_.RunUntilIdle();

  // Set up final expectations for seek.
  EXPECT_CALL(*this, SeekFinished());
  EXPECT_CALL(*rendition_ptr, Seek(_))
      .WillOnce(Return(ManifestDemuxer::SeekState::kIsReady));
  EXPECT_CALL(*mock_dsp_, AbortPendingReads(_)).WillOnce(RunOnceClosure<0>());

  // Finish the adaptation, seek should complete.
  std::move(continue_adaptation).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestSeekDuringTimeUpdate) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the time update, and hold it from finishing.
  base::OnceClosure continue_update =
      StartAndCaptureTimeUpdate(rendition_ptr, base::Seconds(10));

  // Start a seek. It should wait while the update is pending.
  EXPECT_CALL(*this, SeekFinished()).Times(0);
  EXPECT_CALL(*rendition_ptr, Seek(_)).Times(0);
  EXPECT_CALL(*mock_dsp_, AbortPendingReads(_)).Times(0);
  engine_->Seek(
      base::Seconds(10),
      base::BindOnce(
          [](base::OnceClosure cb, ManifestDemuxer::SeekResponse resp) {
            ASSERT_TRUE(resp.has_value());
            std::move(cb).Run();
          },
          base::BindOnce(&HlsManifestDemuxerEngineTest::SeekFinished,
                         base::Unretained(this))));
  task_environment_.RunUntilIdle();

  // Set up final expectations for seek.
  EXPECT_CALL(*this, SeekFinished());
  EXPECT_CALL(*rendition_ptr, Seek(_))
      .WillOnce(Return(ManifestDemuxer::SeekState::kIsReady));
  EXPECT_CALL(*mock_dsp_, AbortPendingReads(_)).WillOnce(RunOnceClosure<0>());

  // Finish the update, seek should complete.
  std::move(continue_update).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestAdaptDuringTimeUpdate) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the time update, and hold it from finishing.
  base::OnceClosure continue_update =
      StartAndCaptureTimeUpdate(rendition_ptr, base::Seconds(10));

  // Start an adaptation. It should wait while the update is pending.
  ExpectNoNetworkRequests();
  engine_->UpdateNetworkSpeed(1380000);
  task_environment_.RunUntilIdle();

  // When the update finishes, the adaptation requests the low quality stream.
  BindUrlAssignmentThunk<StringHlsDataSourceStreamFactory>(
      "http://example.com/low.m3u8", kSimpleMediaPlaylist);
  std::move(continue_update).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestAdaptDuringSeek) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the seek, and hold it so it can't finish.
  base::OnceClosure continue_seek = StartAndCaptureSeek(rendition_ptr);

  // Start an adaptation. It should wait while the seek is pending.
  ExpectNoNetworkRequests();
  engine_->UpdateNetworkSpeed(1380000);
  task_environment_.RunUntilIdle();

  // When the seek finishes, the adaptation requests the low quality stream.
  BindUrlAssignmentThunk<StringHlsDataSourceStreamFactory>(
      "http://example.com/low.m3u8", kSimpleMediaPlaylist);
  EXPECT_CALL(*this, SeekFinished());
  std::move(continue_seek).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestTimeUpdateDuringAdaptation) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the adaptation and hold it from finishing.
  base::OnceClosure continue_adaptation = StartAndCaptureNetworkAdaptation(
      rendition_ptr, "http://example.com/low.m3u8", kSimpleMediaPlaylist,
      1380000);

  // Start the time update
  EXPECT_CALL(*rendition_ptr, CheckState(_, _, _)).Times(0);
  engine_->OnTimeUpdate(base::Seconds(0), 0.0,
                        base::BindOnce([](base::TimeDelta r) {
                          ASSERT_EQ(r, base::Seconds(10));
                        }));
  task_environment_.RunUntilIdle();

  // Set expectations for update finishing.
  EXPECT_CALL(*rendition_ptr, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(10)));

  // Finish adaptation.
  std::move(continue_adaptation).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestTimeUpdateDuringSeek) {
  auto* rendition_ptr = SetUpInterruptTest();

  // Start the seek and hold it from finishing.
  base::OnceClosure continue_seek = StartAndCaptureSeek(rendition_ptr);

  // Start the time update
  EXPECT_CALL(*rendition_ptr, CheckState(_, _, _)).Times(0);
  engine_->OnTimeUpdate(base::Seconds(0), 0.0,
                        base::BindOnce([](base::TimeDelta r) {
                          ASSERT_EQ(r, base::Seconds(10));
                        }));
  task_environment_.RunUntilIdle();

  // Set expectations for update finishing.
  EXPECT_CALL(*rendition_ptr, CheckState(_, _, _))
      .WillOnce(RunOnceCallback<2>(base::Seconds(10)));

  // Finish seek.
  EXPECT_CALL(*this, SeekFinished());
  std::move(continue_seek).Run();
  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestEndOfStreamAfterAllFetched) {
  // All the expectations set during the initialization process.
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  EXPECT_CALL(*mock_mdeh_, SetDuration(9.009));
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));

  // We can't use `BindUrlToDataSource` here, since it can't re-create streams
  // like we need it to. The network requests are in order:
  // - manifest.m3u8 - main manifest
  // - first.ts      - request for the first few bytes to do codec detection
  // - first.ts      - request for chunks of data to add to ChunkDemuxer
  std::string bitstream = "hey, this isn't a bitstream!";
  EXPECT_CALL(*mock_dsp_,
              ReadFromCombinedUrlQueue(
                  SingleSegmentQueue("http://media.example.com/manifest.m3u8",
                                     std::nullopt),
                  _))
      .WillOnce(RunOnceCallback<1>(
          StringHlsDataSourceStreamFactory::CreateStream(kShortMediaPlaylist)));
  EXPECT_CALL(
      *mock_dsp_,
      ReadFromCombinedUrlQueue(
          SingleSegmentQueue("http://media.example.com/first.ts", std::nullopt),
          _))
      .WillOnce(RunOnceCallback<1>(
          StringHlsDataSourceStreamFactory::CreateStream(bitstream)));

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
  EXPECT_CALL(*mock_mdeh_, AppendAndParseData("primary", _, _,
                                              base::as_byte_span(bitstream)))
      .WillOnce(Return(true));

  // Finally, and EndOfStream call happens:
  EXPECT_CALL(*mock_mdeh_, SetEndOfStream());

  // And then teardown:
  EXPECT_CALL(*mock_mdeh_, RemoveRole("primary"));

  // Setup with a mock codec detector - this will set all the roles, duration,
  // modes, and also make a request for the manifest and the first segment.
  InitializeEngine();
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

TEST_F(HlsManifestDemuxerEngineTest, TestEndOfStreamPropagatesOnce) {
  auto rendition1 = std::make_unique<MockHlsRendition>();
  auto rendition2 = std::make_unique<MockHlsRendition>();
  EXPECT_CALL(*rendition1, Stop());
  EXPECT_CALL(*rendition2, Stop());

  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kInvalidMediaPlaylist);
  EXPECT_CALL(*mock_mdeh_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_PARSE)));
  EXPECT_CALL(*this, MockInitComplete(_)).Times(0);
  InitializeEngine();
  task_environment_.RunUntilIdle();

  // Start with one rendition, to demonstrate that when it ends/starts, that
  // event always bubbles up.
  EXPECT_CALL(*rendition1, GetDuration()).WillOnce(Return(std::nullopt));
  engine_->AddRenditionForTesting("primary", std::move(rendition1));

  EXPECT_CALL(*mock_mdeh_, SetEndOfStream());
  engine_->SetEndOfStream(true);

  EXPECT_CALL(*mock_mdeh_, UnsetEndOfStream());
  engine_->SetEndOfStream(false);

  // Add a second rendition, to demonstrate seeks and reaching end state. Both
  // are currently in the "unended" state.
  EXPECT_CALL(*rendition2, GetDuration()).WillOnce(Return(std::nullopt));
  engine_->AddRenditionForTesting("audio", std::move(rendition2));

  // One rendition reaches end, nothing happens.
  EXPECT_CALL(*mock_mdeh_, SetEndOfStream()).Times(0);
  engine_->SetEndOfStream(true);

  // Once all are ended, host gets notified.
  EXPECT_CALL(*mock_mdeh_, SetEndOfStream());
  engine_->SetEndOfStream(true);

  // during seek, the first rendition goes unended - this notifies the host.
  EXPECT_CALL(*mock_mdeh_, UnsetEndOfStream());
  engine_->SetEndOfStream(false);

  // during seek, the second rendition goes unended - this does nothing, as
  // the host already knows the stream is unended.
  EXPECT_CALL(*mock_mdeh_, UnsetEndOfStream()).Times(0);
  engine_->SetEndOfStream(false);

  task_environment_.RunUntilIdle();
}

TEST_F(HlsManifestDemuxerEngineTest, TestOriginTainting) {
  EXPECT_CALL(*mock_mdeh_, SetSequenceMode("primary", true));
  EXPECT_CALL(*mock_mdeh_, SetDuration(21.021));
  EXPECT_CALL(*mock_mdeh_,
              AddRole("primary", RelaxedParserSupportedType::kMP2T));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8", kSimpleMultivariantPlaylist,
      /*taint_origin=*/true);
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://example.com/hi.m3u8", kSimpleMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  InitializeEngine();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(engine_->WouldTaintOrigin());
}

TEST_F(HlsManifestDemuxerEngineTest, TestInitialSegmentEncrypted) {
  std::string cleartext = "G <- 0x47 (G) is the sentinal byte for TS content";
  std::string ciphertext;
  std::unique_ptr<crypto::SymmetricKey> key;
  std::tie(ciphertext, key) = Encrypt(cleartext, "ffffffffffffffff");
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/manifest.m3u8",
      kLiveFullEncryptedMediaPlaylist);
  EXPECT_CALL(*this, MockInitComplete(HasStatusCode(PIPELINE_OK)));
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/K", key->key());
  BindUrlToDataSource<StringHlsDataSourceStreamFactory>(
      "http://media.example.com/13979.js", ciphertext);
  InitializeEngine();
  task_environment_.RunUntilIdle();
}

}  // namespace media
