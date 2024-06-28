// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_state.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/test_helpers.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using base::test::RunClosure;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;

namespace {

AudioDecoderConfig CreateAudioConfig(AudioCodec codec) {
  return AudioDecoderConfig(codec, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, 1000, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
}

VideoDecoderConfig CreateVideoConfig(VideoCodec codec, int w, int h) {
  gfx::Size size(w, h);
  gfx::Rect visible_rect(size);
  return VideoDecoderConfig(codec, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace::REC709(), kNoTransformation, size,
                            visible_rect, size, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
}

void AddAudioTrack(std::unique_ptr<MediaTracks>& t, AudioCodec codec, int id) {
  t->AddAudioTrack(CreateAudioConfig(codec), true, id, MediaTrack::Kind(),
                   MediaTrack::Label(), MediaTrack::Language());
}

void AddVideoTrack(std::unique_ptr<MediaTracks>& t, VideoCodec codec, int id) {
  t->AddVideoTrack(CreateVideoConfig(codec, 16, 16), true, id,
                   MediaTrack::Kind(), MediaTrack::Label(),
                   MediaTrack::Language());
}

}  // namespace

class SourceBufferStateTest : public ::testing::Test {
 public:
  SourceBufferStateTest() : mock_stream_parser_(nullptr) {}

  std::unique_ptr<SourceBufferState> CreateSourceBufferState() {
    std::unique_ptr<FrameProcessor> frame_processor =
        std::make_unique<FrameProcessor>(
            base::BindRepeating(&SourceBufferStateTest::OnUpdateDuration,
                                base::Unretained(this)),
            &media_log_);
    mock_stream_parser_ = new testing::StrictMock<MockStreamParser>();
    return base::WrapUnique(new SourceBufferState(
        base::WrapUnique(mock_stream_parser_.get()), std::move(frame_processor),
        base::BindRepeating(&SourceBufferStateTest::CreateDemuxerStream,
                            base::Unretained(this)),
        &media_log_));
  }

  std::unique_ptr<SourceBufferState> CreateAndInitSourceBufferState(
      const std::string& expected_codecs) {
    std::unique_ptr<SourceBufferState> sbs = CreateSourceBufferState();
    // Instead of using SaveArg<> to update |new_config_cb_| when mocked Init is
    // called, we use a lambda because SaveArg<> doesn't work if any of the
    // mocked method's arguments are move-only type.
    EXPECT_CALL(*mock_stream_parser_, Init(_, _, _, _, _, _, _))
        .WillOnce([&](auto init_cb, auto config_cb, auto new_buffers_cb,
                      auto encrypted_media_init_data_cb, auto new_segment_cb,
                      auto end_of_segment_cb,
                      auto media_log) { new_config_cb_ = config_cb; });
    sbs->Init(base::BindOnce(&SourceBufferStateTest::SourceInitDone,
                             base::Unretained(this)),
              expected_codecs,
              base::BindRepeating(
                  &SourceBufferStateTest::StreamParserEncryptedInitData,
                  base::Unretained(this)));

    sbs->SetTracksWatcher(base::BindRepeating(
        &SourceBufferStateTest::OnMediaTracksUpdated, base::Unretained(this)));

    // These tests are not expected to issue any parse warnings.
    EXPECT_CALL(*this, OnParseWarningMock(_)).Times(0);

    sbs->SetParseWarningCallback(base::BindRepeating(
        &SourceBufferStateTest::OnParseWarningMock, base::Unretained(this)));

    return sbs;
  }

  // Emulates appending and parsing some data to the SourceBufferState, since
  // OnNewConfigs can only be invoked when parse is in progress.
  bool AppendDataAndReportTracks(const std::unique_ptr<SourceBufferState>& sbs,
                                 std::unique_ptr<MediaTracks> tracks) {
    const uint8_t kStreamData[] = "stream_data";
    base::span<const uint8_t> stream_data = base::make_span(kStreamData);
    base::TimeDelta t;

    // Ensure `stream_data` fits within one StreamParser::Parse() call.
    CHECK_GT(StreamParser::kMaxPendingBytesPerParse,
             static_cast<int>(stream_data.size_bytes()));

    bool new_configs_result = false;

    EXPECT_CALL(*mock_stream_parser_, AppendToParseBuffer(stream_data))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_stream_parser_,
                Parse(StreamParser::kMaxPendingBytesPerParse))
        .WillOnce(
            DoAll(InvokeWithoutArgs([&] {
                    new_configs_result = new_config_cb_.Run(std::move(tracks));
                  }),
                  /* Indicate successful parse with no uninspected data. */
                  Return(StreamParser::ParseStatus::kSuccess)));

    EXPECT_TRUE(sbs->AppendToParseBuffer(stream_data));
    EXPECT_EQ(StreamParser::ParseStatus::kSuccess,
              sbs->RunSegmentParserLoop(t, t, &t));

    return new_configs_result;
  }

  MOCK_METHOD1(OnParseWarningMock, void(const SourceBufferParseWarning));
  MOCK_METHOD1(OnUpdateDuration, void(base::TimeDelta));

  MOCK_METHOD1(SourceInitDone, void(const StreamParser::InitParameters&));
  MOCK_METHOD2(StreamParserEncryptedInitData,
               void(EmeInitDataType, const std::vector<uint8_t>&));

  MOCK_METHOD1(MediaTracksUpdatedMock, void(std::unique_ptr<MediaTracks>&));
  void OnMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks) {
    MediaTracksUpdatedMock(tracks);
  }

  ChunkDemuxerStream* CreateDemuxerStream(DemuxerStream::Type type) {
    static unsigned track_id = 0;
    demuxer_streams_.push_back(base::WrapUnique(new ChunkDemuxerStream(
        type, MediaTrack::Id(base::NumberToString(++track_id)))));
    return demuxer_streams_.back().get();
  }

  testing::StrictMock<MockMediaLog> media_log_;
  std::vector<std::unique_ptr<ChunkDemuxerStream>> demuxer_streams_;
  raw_ptr<MockStreamParser, DanglingUntriaged> mock_stream_parser_;
  StreamParser::NewConfigCB new_config_cb_;
};

TEST_F(SourceBufferStateTest, InitSourceBufferWithRelaxedCodecChecks) {
  std::unique_ptr<SourceBufferState> sbs = CreateSourceBufferState();
  EXPECT_CALL(*mock_stream_parser_, Init(_, _, _, _, _, _, _))
      .WillOnce([&](auto init_cb, auto config_cb, auto new_buffers_cb,
                    auto encrypted_media_init_data_cb, auto new_segment_cb,
                    auto end_of_segment_cb,
                    auto media_log) { new_config_cb_ = config_cb; });

  sbs->Init(
      base::BindOnce(&SourceBufferStateTest::SourceInitDone,
                     base::Unretained(this)),
      std::nullopt,
      base::BindRepeating(&SourceBufferStateTest::StreamParserEncryptedInitData,
                          base::Unretained(this)));

  sbs->SetTracksWatcher(base::BindRepeating(
      &SourceBufferStateTest::OnMediaTracksUpdated, base::Unretained(this)));

  EXPECT_CALL(*this, OnParseWarningMock(_)).Times(0);

  sbs->SetParseWarningCallback(base::BindRepeating(
      &SourceBufferStateTest::OnParseWarningMock, base::Unretained(this)));

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kVorbis, 1);

  EXPECT_FOUND_CODEC_NAME(Audio, "vorbis");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, InitSingleAudioTrack) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vorbis");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kVorbis, 1);

  EXPECT_FOUND_CODEC_NAME(Audio, "vorbis");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, InitSingleVideoTrack) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vp8");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddVideoTrack(tracks, VideoCodec::kVP8, 1);

  EXPECT_FOUND_CODEC_NAME(Video, "vp8");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, InitMultipleTracks) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vorbis,vp8,opus,vp9");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kVorbis, 1);
  AddAudioTrack(tracks, AudioCodec::kOpus, 2);
  AddVideoTrack(tracks, VideoCodec::kVP8, 3);
  AddVideoTrack(tracks, VideoCodec::kVP9, 4);

  EXPECT_FOUND_CODEC_NAME(Audio, "vorbis");
  EXPECT_FOUND_CODEC_NAME(Audio, "opus");
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_FOUND_CODEC_NAME(Video, "vp8");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, AudioStreamMismatchesExpectedCodecs) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("opus");
  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kVorbis, 1);
  EXPECT_MEDIA_LOG(InitSegmentMismatchesMimeType("Audio", "vorbis"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, VideoStreamMismatchesExpectedCodecs) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vp9");
  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddVideoTrack(tracks, VideoCodec::kVP8, 1);
  EXPECT_MEDIA_LOG(InitSegmentMismatchesMimeType("Video", "vp8"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, MissingExpectedAudioStream) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("opus,vp9");
  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddVideoTrack(tracks, VideoCodec::kVP9, 1);
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("opus"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, MissingExpectedVideoStream) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("opus,vp9");
  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  tracks->AddAudioTrack(CreateAudioConfig(AudioCodec::kOpus), true, 1,
                        MediaTrack::Kind(), MediaTrack::Label(),
                        MediaTrack::Language());
  EXPECT_FOUND_CODEC_NAME(Audio, "opus");
  EXPECT_MEDIA_LOG(InitSegmentMissesExpectedTrack("vp9"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks)));
}

TEST_F(SourceBufferStateTest, TrackIdsChangeInSecondInitSegment) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("opus,vp9");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kOpus, 1);
  AddVideoTrack(tracks, VideoCodec::kVP9, 2);
  EXPECT_FOUND_CODEC_NAME(Audio, "opus");
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  AppendDataAndReportTracks(sbs, std::move(tracks));

  // This second set of tracks have bytestream track ids that differ from the
  // first init segment above (audio track id 1 -> 3, video track id 2 -> 4).
  // Bytestream track ids are allowed to change when there is only a single
  // track of each type.
  std::unique_ptr<MediaTracks> tracks2(new MediaTracks());
  AddAudioTrack(tracks2, AudioCodec::kOpus, 3);
  AddVideoTrack(tracks2, VideoCodec::kVP9, 4);
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  AppendDataAndReportTracks(sbs, std::move(tracks2));
}

TEST_F(SourceBufferStateTest, TrackIdChangeWithTwoAudioTracks) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vorbis,opus");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kVorbis, 1);
  AddAudioTrack(tracks, AudioCodec::kOpus, 2);
  EXPECT_FOUND_CODEC_NAME(Audio, "vorbis");
  EXPECT_FOUND_CODEC_NAME(Audio, "opus");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));

  // Since we have two audio tracks, bytestream track ids must match the first
  // init segment.
  std::unique_ptr<MediaTracks> tracks2(new MediaTracks());
  AddAudioTrack(tracks2, AudioCodec::kVorbis, 1);
  AddAudioTrack(tracks2, AudioCodec::kOpus, 2);
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks2)));

  // Emulate the situation where bytestream track ids have changed in the third
  // init segment. This must cause failure in the OnNewConfigs.
  std::unique_ptr<MediaTracks> tracks3(new MediaTracks());
  AddAudioTrack(tracks3, AudioCodec::kVorbis, 1);
  AddAudioTrack(tracks3, AudioCodec::kOpus, 3);
  EXPECT_MEDIA_LOG(UnexpectedTrack("audio", "3"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks3)));
}

TEST_F(SourceBufferStateTest, TrackIdChangeWithTwoVideoTracks) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vp8,vp9");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddVideoTrack(tracks, VideoCodec::kVP8, 1);
  AddVideoTrack(tracks, VideoCodec::kVP9, 2);
  EXPECT_FOUND_CODEC_NAME(Video, "vp8");
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks)));

  // Since we have two video tracks, bytestream track ids must match the first
  // init segment.
  std::unique_ptr<MediaTracks> tracks2(new MediaTracks());
  AddVideoTrack(tracks2, VideoCodec::kVP8, 1);
  AddVideoTrack(tracks2, VideoCodec::kVP9, 2);
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  EXPECT_TRUE(AppendDataAndReportTracks(sbs, std::move(tracks2)));

  // Emulate the situation where bytestream track ids have changed in the third
  // init segment. This must cause failure in the OnNewConfigs.
  std::unique_ptr<MediaTracks> tracks3(new MediaTracks());
  AddVideoTrack(tracks3, VideoCodec::kVP8, 1);
  AddVideoTrack(tracks3, VideoCodec::kVP9, 3);
  EXPECT_MEDIA_LOG(UnexpectedTrack("video", "3"));
  EXPECT_FALSE(AppendDataAndReportTracks(sbs, std::move(tracks3)));
}

TEST_F(SourceBufferStateTest, TrackIdsSwappedInSecondInitSegment) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("opus,vp9");

  std::unique_ptr<MediaTracks> tracks(new MediaTracks());
  AddAudioTrack(tracks, AudioCodec::kOpus, 1);
  AddVideoTrack(tracks, VideoCodec::kVP9, 2);
  EXPECT_FOUND_CODEC_NAME(Audio, "opus");
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  AppendDataAndReportTracks(sbs, std::move(tracks));

  // Track ids are swapped in the second init segment.
  std::unique_ptr<MediaTracks> tracks2(new MediaTracks());
  AddAudioTrack(tracks2, AudioCodec::kOpus, 2);
  AddVideoTrack(tracks2, VideoCodec::kVP9, 1);
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));
  AppendDataAndReportTracks(sbs, std::move(tracks2));
}

}  // namespace media
