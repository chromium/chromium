// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/fake_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Lt;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_os_info_override_win.h"
#include "media/gpu/windows/mf_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)) && \
    BUILDFLAG(USE_PROPRIETARY_CODECS)
#define HAS_AAC_ENCODER 1
#endif

namespace blink {

static String TestVideoTrackId() {
  return "video_track_id";
}

static String TestAudioTrackId() {
  return "audio_track_id";
}
static const int kTestAudioChannels = 2;
static const int kTestAudioSampleRate = 48000;
static const int kTestAudioBufferDurationMs = 10;
// Opus works with 60ms buffers, so 6 MediaStreamAudioTrack Buffers are needed
// to encode one output buffer.
static const int kRatioOpusToTestAudioBuffers = 6;

struct MediaRecorderTestParams {
  const bool mp4_enabled;
  const bool has_video;
  const bool has_audio;
  const char* const mime_type;
  const char* const codecs;
  const bool encoder_supports_alpha;
  const bool use_mp4_muxer = false;
};

// Array of valid combinations of video/audio/codecs and expected collected
// encoded sizes to use for parameterizing MediaRecorderHandlerTest.
static const MediaRecorderTestParams kMediaRecorderTestParams[] = {
    {false, true, false, "video/webm", "vp8", true},
    {false, true, false, "video/webm", "vp9", true},
    {false, true, false, "video/webm", "av01", false},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {false, true, false, "video/x-matroska", "avc1", false},
#endif
    {false, false, true, "audio/webm", "opus", true},
    {false, false, true, "audio/webm", "", true},  // Should default to opus.
    {false, false, true, "audio/webm", "pcm", true},
    {false, true, true, "video/webm", "vp9,opus", true},
    // mp4 enabled.
    {true, true, false, "video/webm", "vp8", true},
    {true, true, false, "video/webm", "vp9", true},
    {true, true, false, "video/webm", "av01", false},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {true, true, false, "video/x-matroska", "avc1", false},
    {true, true, false, "video/mp4", "avc1", false, true},
    {true, true, true, "video/mp4", "avc1,mp4a.40.2", false, true},
    {true, false, true, "audio/mp4", "mp4a.40.2", false, true},
    {true, true, true, "video/mp4", "avc1,opus", false, true},
    {true, true, true, "video/mp4", "vp9,mp4a.40.2", false, true},
#endif
    {true, false, true, "audio/webm", "opus", true},
    {true, false, true, "audio/webm", "", true},  // Should default to opus.
    {true, false, true, "audio/webm", "pcm", true},
    {true, true, true, "video/webm", "vp9,opus", true},
    {true, false, true, "audio/mp4", "opus", false, true},
    {true, true, false, "video/mp4", "vp9", false, true},
    {true, true, true, "video/mp4", "vp9,opus", false, true},
};

MediaStream* CreateMediaStream(V8TestingScope& scope) {
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "sourceId", MediaStreamSource::kTypeAudio, "sourceName", false,
      /*platform_source=*/nullptr);
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      "audioTrack", source,
      std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/true));

  auto* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component);

  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);

  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);

  return stream;
}

class MockMediaRecorder : public MediaRecorder {
 public:
  explicit MockMediaRecorder(V8TestingScope& scope)
      : MediaRecorder(scope.GetExecutionContext(),
                      CreateMediaStream(scope),
                      MediaRecorderOptions::Create(),
                      scope.GetExceptionState()) {}
  ~MockMediaRecorder() override = default;

  MOCK_METHOD(void, WriteData, (base::span<const uint8_t>, bool, ErrorEvent*));
  MOCK_METHOD(void, OnError, (DOMExceptionCode code, const String& message));
};

class MediaRecorderHandlerFixture : public ScopedMockOverlayScrollbars {
 public:
  MediaRecorderHandlerFixture(bool has_video, bool has_audio)
      : has_video_(has_video),
        has_audio_(has_audio),
        media_recorder_handler_(MakeGarbageCollected<MediaRecorderHandler>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            KeyFrameRequestProcessor::Configuration())),
        audio_source_(kTestAudioChannels,
                      440 /* freq */,
                      kTestAudioSampleRate) {
    EXPECT_FALSE(media_recorder_handler_->recording_);

    registry_.Init();
  }

  ~MediaRecorderHandlerFixture() {
    registry_.reset();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  bool recording() const { return media_recorder_handler_->recording_; }
  bool hasVideoRecorders() const {
    return !media_recorder_handler_->video_recorders_.empty();
  }
  bool hasAudioRecorders() const {
    return !media_recorder_handler_->audio_recorders_.empty();
  }

  bool IsTargetAudioCodecSupported(const String& codecs) {
    if (codecs.Find("mp4a.40.2") != kNotFound) {
#if !defined(HAS_AAC_ENCODER)
      return false;
#else
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      return media::MojoAudioEncoder::IsSupported(media::AudioCodec::kAAC);
#else
      return false;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#endif  // !defined(HAS_AAC_ENCODER)
    }

    return true;
  }

  bool IsAv1CodecSupported(const String codecs) {
#if BUILDFLAG(ENABLE_LIBAOM)
    return true;
#else
    return codecs.Find("av1") != kNotFound && codecs.Find("av01") != kNotFound;
#endif
  }

  WeakCell<AudioTrackRecorder::CallbackInterface>* GetAudioCallbackInterface() {
    return media_recorder_handler_->audio_recorders_[0]
        ->callback_interface_for_testing();
  }

  WeakCell<VideoTrackRecorder::CallbackInterface>* GetVideoCallbackInterface() {
    return media_recorder_handler_->video_recorders_[0]->callback_interface();
  }

  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame) {
    media_recorder_handler_->OnVideoFrameForTesting(std::move(frame),
                                                    base::TimeTicks::Now());
  }

  void OnEncodedVideoForTesting(
      const media::Muxer::VideoParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      base::TimeTicks timestamp,
      std::optional<media::VideoEncoder::CodecDescription> codec_description =
          std::nullopt) {
    media_recorder_handler_->OnEncodedVideo(params, std::move(encoded_data),
                                            std::move(codec_description),
                                            timestamp);
  }

  void OnEncodedAudioForTesting(
      const media::AudioParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      base::TimeTicks timestamp) {
    media::AudioEncoder::CodecDescription codec_description = {99};
    media_recorder_handler_->OnEncodedAudio(params, std::move(encoded_data),
                                            std::move(codec_description),
                                            timestamp);
  }

  void OnEncodedAudioNoCodeDescriptionForTesting(
      const media::AudioParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      base::TimeTicks timestamp) {
    media_recorder_handler_->OnEncodedAudio(params, std::move(encoded_data),
                                            std::nullopt, timestamp);
  }

  void OnAudioBusForTesting(const media::AudioBus& audio_bus) {
    media_recorder_handler_->OnAudioBusForTesting(audio_bus,
                                                  base::TimeTicks::Now());
  }
  void SetAudioFormatForTesting(const media::AudioParameters& params) {
    media_recorder_handler_->SetAudioFormatForTesting(params);
  }

  void AddVideoTrack() {
    video_source_ = registry_.AddVideoTrack(TestVideoTrackId());
  }

  void AddTracks() {
    // Avoid issues with non-parameterized tests by calling this outside of ctr.
    if (has_video_)
      AddVideoTrack();
    if (has_audio_)
      registry_.AddAudioTrack(TestAudioTrackId());
  }

  void ForceOneErrorInWebmMuxer() {
    static_cast<media::WebmMuxer*>(
        media_recorder_handler_->muxer_adapter_->GetMuxerForTesting())
        ->ForceOneLibWebmErrorForTesting();
  }

  std::unique_ptr<media::AudioBus> NextAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        kTestAudioChannels,
        kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000));
    audio_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                             bus.get());
    return bus;
  }

  void OnEncodedH264VideoForTesting(
      base::TimeTicks timestamp,
      std::optional<media::VideoEncoder::CodecDescription> codec_description =
          std::nullopt) {
    // It provides valid h264 stream.
    if (h264_video_stream_.empty()) {
      base::MemoryMappedFile mapped_h264_file;
      LoadEncodedFile("h264-320x180-frame-0", mapped_h264_file);
      h264_video_stream_ =
          base::HeapArray<uint8_t>::CopiedFrom(mapped_h264_file.bytes());
    }
    media::Muxer::VideoParameters video_params(
        gfx::Size(), 1, media::VideoCodec::kH264, gfx::ColorSpace());
    auto buffer = media::DecoderBuffer::CopyFrom(h264_video_stream_);
    std::string alpha_data = "alpha";
    buffer->WritableSideData().alpha_data.assign(alpha_data.begin(),
                                                 alpha_data.end());
    buffer->set_is_key_frame(true);
    OnEncodedVideoForTesting(video_params, buffer, timestamp,
                             std::move(codec_description));
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  void PopulateAVCDecoderConfiguration(
      std::vector<uint8_t>& codec_description) {
    // copied from box_reader_unittest.cc.
    std::vector<uint8_t> test_data{
        0x1,        // configurationVersion = 1
        0x64,       // AVCProfileIndication = 100
        0x0,        // profile_compatibility = 0
        0xc,        // AVCLevelIndication = 10
        0xff,       // lengthSizeMinusOne = 3
        0xe1,       // numOfSequenceParameterSets = 1
        0x0, 0x19,  // sequenceParameterSetLength = 25

        // sequenceParameterSet
        0x67, 0x64, 0x0, 0xc, 0xac, 0xd9, 0x41, 0x41, 0xfb, 0x1, 0x10, 0x0, 0x0,
        0x3, 0x0, 0x10, 0x0, 0x0, 0x3, 0x1, 0x40, 0xf1, 0x42, 0x99, 0x60,

        0x1,       // numOfPictureParameterSets
        0x0, 0x6,  // pictureParameterSetLength = 6
        0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0,

        0xfd,  // chroma_format = 1
        0xf8,  // bit_depth_luma_minus8 = 0
        0xf8,  // bit_depth_chroma_minus8 = 0
        0x0,   // numOfSequanceParameterSetExt = 0
    };

    media::mp4::AVCDecoderConfigurationRecord avc_config;
    ASSERT_TRUE(
        avc_config.Parse(test_data.data(), static_cast<int>(test_data.size())));
    ASSERT_TRUE(avc_config.Serialize(codec_description));
  }
#endif

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  MockMediaStreamRegistry registry_;
  bool has_video_;
  bool has_audio_;
  Persistent<MediaRecorderHandler> media_recorder_handler_;
  media::SineWaveAudioSource audio_source_;
  raw_ptr<MockMediaStreamVideoSource, DanglingUntriaged> video_source_ =
      nullptr;
  base::HeapArray<uint8_t> h264_video_stream_;

 private:
  void LoadEncodedFile(std::string_view filename,
                       base::MemoryMappedFile& mapped_stream) {
    base::FilePath file_path = GetTestDataFilePath(filename);

    ASSERT_TRUE(mapped_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  }

  base::FilePath GetTestDataFilePath(std::string_view name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .AppendASCII(name);
    return file_path;
  }
};

class MediaRecorderHandlerTest : public TestWithParam<MediaRecorderTestParams>,
                                 public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerTest()
      : MediaRecorderHandlerFixture(GetParam().has_video,
                                    GetParam().has_audio) {
    if (GetParam().mp4_enabled) {
      scoped_feature_list_.InitAndEnableFeature(kMediaRecorderEnableMp4Muxer);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kMediaRecorderEnableMp4Muxer);
    }
  }

  bool IsCodecSupported() {
#if !BUILDFLAG(ENABLE_OPENH264)
    // Test requires OpenH264 encoder. It can't use the VEA encoder.
    if (String(GetParam().codecs).Find("avc1") != kNotFound) {
      return false;
    }
#endif
#if !BUILDFLAG(ENABLE_LIBAOM)
    if (std::string(GetParam().codecs) == "av01") {
      return false;
    }
#endif
    return true;
  }

  bool IsStreamWriteSupported() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    // TODO(crbug/1480178): Support valid   codec_description  parameter
    // for OnEncodedVideo/Audio to support real stream write.
    if (EqualIgnoringASCIICase(GetParam().mime_type, "video/mp4") ||
        EqualIgnoringASCIICase(GetParam().mime_type, "audio/mp4")) {
      return false;
    }
#endif
    return true;
  }

  bool IsAvc1CodecSupported(const String codecs) {
    return codecs.Find("avc1") != kNotFound;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that canSupportMimeType() works as expected, by sending supported
// combinations and unsupported ones.
TEST_P(MediaRecorderHandlerTest, CanSupportMimeType) {
  const String unsupported_mime_type("video/mpeg");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      unsupported_mime_type, String()));

  const String mime_type_video("video/webm");
  EXPECT_TRUE(
      media_recorder_handler_->CanSupportMimeType(mime_type_video, String()));
  const String mime_type_video_uppercase("video/WEBM");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video_uppercase, String()));
  const String example_good_codecs_1("vp8");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_1));
  const String example_good_codecs_2("vp9,opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_2));
  const String example_good_codecs_3("VP9,opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_3));
  const String example_good_codecs_4("H264");
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_4));
#else
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_4));
#endif

  const String example_unsupported_codecs_1("daala");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_unsupported_codecs_1));

  const String mime_type_audio("audio/webm");
  EXPECT_TRUE(
      media_recorder_handler_->CanSupportMimeType(mime_type_audio, String()));
  const String example_good_codecs_5("opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_5));
  const String example_good_codecs_6("OpUs");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_6));
  const String example_good_codecs_7("pcm");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_7));

  const String example_good_codecs_8("AV01,opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_5));

  const String example_unsupported_codecs_2("vorbis");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_unsupported_codecs_2));
}

// Checks that it uses the specified bitrate mode.
TEST_P(MediaRecorderHandlerTest, SupportsBitrateMode) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  if (!IsAv1CodecSupported(codecs)) {
    return;
  }

  if (!IsTargetAudioCodecSupported(codecs)) {
    return;
  }

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_EQ(media_recorder_handler_->AudioBitrateMode(),
            AudioTrackRecorder::BitrateMode::kVariable);

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kConstant));
  EXPECT_EQ(media_recorder_handler_->AudioBitrateMode(),
            AudioTrackRecorder::BitrateMode::kConstant);
}

// Checks that the initialization-destruction sequence works fine.
TEST_P(MediaRecorderHandlerTest, InitializeFailedWhenMP4MuxerFeatureDisabled) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  if (!IsAv1CodecSupported(codecs)) {
    return;
  }

  if (!IsTargetAudioCodecSupported(codecs)) {
    return;
  }

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());

  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));
  EXPECT_TRUE(recording());

  EXPECT_TRUE(hasVideoRecorders() || !GetParam().has_video);
  EXPECT_TRUE(hasAudioRecorders() || !GetParam().has_audio);

  media_recorder_handler_->Stop();
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());
}

// Sends 2 opaque frames and 1 transparent frame and expects them as WebM
// contained encoded data in writeData().
TEST_P(MediaRecorderHandlerTest, EncodeVideoFrames) {
  // Video-only test unless it is Mp4 muxer that needs `mp4a.40.2` audio codec.
  if ((GetParam().has_audio && !GetParam().use_mp4_muxer) ||
      !IsCodecSupported()) {
    return;
  }

  if (!GetParam().has_video) {
    return;
  }

  if (!IsTargetAudioCodecSupported(GetParam().codecs)) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  if (GetParam().use_mp4_muxer) {
    {
      const size_t kMfraBoxSize = 76u;
      base::RunLoop run_loop;
      // WriteData is called as many as fragments (`moof` box) in addition
      // to 3 times of `ftyp`, `moov`, `mfra` boxes.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kMfraBoxSize)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kMfraBoxSize)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(kMfraBoxSize), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

      OnVideoFrameForTesting(video_frame);
      test::RunDelayedTasks(base::Seconds(2));

      // Mp4Muxer will flush when it is destroyed.
      media_recorder_handler_->Stop();
      run_loop.Run();
    }
  } else {
    {
      const size_t kEncodedSizeThreshold = 16;
      base::RunLoop run_loop;
      // writeData() is pinged a number of times as the WebM header is written;
      // the last time it is called it has the encoded data.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

      OnVideoFrameForTesting(video_frame);
      run_loop.Run();
    }
    Mock::VerifyAndClearExpectations(recorder);
    {
      const size_t kEncodedSizeThreshold = 12;
      base::RunLoop run_loop;
      // The second time around writeData() is called a number of times to write
      // the WebM frame header, and then is pinged with the encoded data.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

      OnVideoFrameForTesting(video_frame);
      run_loop.Run();
    }
    Mock::VerifyAndClearExpectations(recorder);
    {
      const scoped_refptr<media::VideoFrame> alpha_frame =
          media::VideoFrame::CreateTransparentFrame(gfx::Size(160, 80));
      const size_t kEncodedSizeThreshold = 16;
      EXPECT_EQ(4u, media::VideoFrame::NumPlanes(alpha_frame->format()));
      base::RunLoop run_loop;
      // The second time around writeData() is called a number of times to write
      // the WebM frame header, and then is pinged with the encoded data.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
      if (GetParam().encoder_supports_alpha) {
        EXPECT_CALL(*recorder,
                    WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
            .Times(AtLeast(1));
        EXPECT_CALL(*recorder,
                    WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
            .Times(1)
            .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
      }
      OnVideoFrameForTesting(alpha_frame);
      run_loop.Run();
    }
    Mock::VerifyAndClearExpectations(recorder);
  }

  media_recorder_handler_->Stop();
}

// Sends 2 frames and expect them as WebM (or MKV) contained encoded audio data
// in writeData().
TEST_P(MediaRecorderHandlerTest, OpusEncodeAudioFrames) {
  // Audio-only test.
  if (GetParam().has_video || !IsStreamWriteSupported()) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));

  InSequence s;
  const std::unique_ptr<media::AudioBus> audio_bus1 = NextAudioBus();
  const std::unique_ptr<media::AudioBus> audio_bus2 = NextAudioBus();

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::Stereo(), kTestAudioSampleRate,
      kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000);
  SetAudioFormatForTesting(params);

  if (GetParam().use_mp4_muxer) {
    const size_t kEncodedSizeThreshold = 48u;

    base::RunLoop run_loop;
    // WriteData is called as many as fragments (`moof` box) in addition
    // to 2 times of `ftyp`, `moov` boxes (no 'mfra'box as it is audio only).
    EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
        .Times(2)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_PCM_LINEAR,
        media::ChannelLayoutConfig::Stereo(), kTestAudioSampleRate,
        kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000);

    // Null codec_description is used for Opus.
    auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("audio"));
    OnEncodedAudioNoCodeDescriptionForTesting(audio_params, buffer,
                                              base::TimeTicks::Now());

    media_recorder_handler_->Stop();

    run_loop.Run();

    Mock::VerifyAndClearExpectations(recorder);
  } else {
    const size_t kEncodedSizeThreshold = 24;
    {
      base::RunLoop run_loop;
      // writeData() is pinged a number of times as the WebM header is written;
      // the last time it is called it has the encoded data.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

      for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i) {
        OnAudioBusForTesting(*audio_bus1);
      }
      run_loop.Run();
    }
    Mock::VerifyAndClearExpectations(recorder);

    {
      base::RunLoop run_loop;
      // The second time around writeData() is called a number of times to write
      // the WebM frame header, and then is pinged with the encoded data.
      EXPECT_CALL(*recorder, WriteData(SizeIs(Lt(kEncodedSizeThreshold)), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

      for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i) {
        OnAudioBusForTesting(*audio_bus2);
      }
      run_loop.Run();
    }
    Mock::VerifyAndClearExpectations(recorder);
  }

  media_recorder_handler_->Stop();
}

// Starts up recording and forces a WebmMuxer's libwebm error.
TEST_P(MediaRecorderHandlerTest, WebmMuxerErrorWhileEncoding) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio || !IsCodecSupported() ||
      !IsStreamWriteSupported() || GetParam().use_mp4_muxer) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  {
    const size_t kEncodedSizeThreshold = 16;
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(SizeIs(Gt(kEncodedSizeThreshold)), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }

  ForceOneErrorInWebmMuxer();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData).Times(0);
    EXPECT_CALL(*recorder, OnError)
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(recorder);

  // Make sure the |media_recorder_handler_| gets destroyed and removing sinks
  // before the MediaStreamVideoTrack dtor, avoiding a DCHECK on a non-empty
  // callback list.
  media_recorder_handler_ = nullptr;
}

// Checks the ActualMimeType() versus the expected.
TEST_P(MediaRecorderHandlerTest, ActualMimeType) {
  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  if (!IsAv1CodecSupported(codecs)) {
    return;
  }

  if (!IsTargetAudioCodecSupported(codecs)) {
    return;
  }

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));

  StringBuilder actual_mime_type;
  actual_mime_type.Append(GetParam().mime_type);
  actual_mime_type.Append(";codecs=");
  if (strlen(GetParam().codecs) != 0u) {
    actual_mime_type.Append(GetParam().codecs);
  } else if (GetParam().has_video) {
    actual_mime_type.Append("vp8");
  } else if (GetParam().has_audio) {
    actual_mime_type.Append("opus");
  }

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(),
            actual_mime_type.ToString());
}

TEST_P(MediaRecorderHandlerTest, PauseRecorderForVideo) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  if (!IsAv1CodecSupported(codecs)) {
    return;
  }

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));

  Mock::VerifyAndClearExpectations(recorder);
  media_recorder_handler_->Pause();

  if (GetParam().use_mp4_muxer) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
    media::Muxer::VideoParameters params(
        gfx::Size(), 1, media::VideoCodec::kH264, gfx::ColorSpace());
    std::vector<uint8_t> codec_description;
    PopulateAVCDecoderConfiguration(codec_description);
    OnEncodedH264VideoForTesting(base::TimeTicks::Now(),
                                 std::move(codec_description));
    media_recorder_handler_->Stop();
#endif
  } else {
    EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
    media::Muxer::VideoParameters params(
        gfx::Size(), 1, media::VideoCodec::kVP9, gfx::ColorSpace());
    if (IsAvc1CodecSupported(codecs)) {
      OnEncodedH264VideoForTesting(base::TimeTicks::Now());
    } else {
      auto buffer =
          media::DecoderBuffer::CopyFrom(base::as_byte_span("vp9 frame"));
      std::string alpha_data = "alpha";
      buffer->WritableSideData().alpha_data.assign(alpha_data.begin(),
                                                   alpha_data.end());
      buffer->set_is_key_frame(true);
      OnEncodedVideoForTesting(params, buffer, base::TimeTicks::Now());
    }
  }

  Mock::VerifyAndClearExpectations(recorder);

  // Make sure the |media_recorder_handler_| gets destroyed and removing sinks
  // before the MediaStreamVideoTrack dtor, avoiding a DCHECK on a non-empty
  // callback list.
  media_recorder_handler_ = nullptr;
}

TEST_P(MediaRecorderHandlerTest, StartStopStartRecorderForVideo) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio || !IsStreamWriteSupported()) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  if (!IsAv1CodecSupported(codecs)) {
    return;
  }

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));
  media_recorder_handler_->Stop();

  Mock::VerifyAndClearExpectations(recorder);
  EXPECT_TRUE(media_recorder_handler_->Start(0, mime_type, 0, 0));

  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media::Muxer::VideoParameters params(gfx::Size(), 1, media::VideoCodec::kVP9,
                                       gfx::ColorSpace());
  if (IsAvc1CodecSupported(codecs)) {
    OnEncodedH264VideoForTesting(base::TimeTicks::Now());
  } else {
    auto buffer =
        media::DecoderBuffer::CopyFrom(base::as_byte_span("vp9 frame"));
    std::string alpha_data = "alpha";
    buffer->WritableSideData().alpha_data.assign(alpha_data.begin(),
                                                 alpha_data.end());
    buffer->set_is_key_frame(true);
    OnEncodedVideoForTesting(params, buffer, base::TimeTicks::Now());
  }

  Mock::VerifyAndClearExpectations(recorder);

  // Make sure the |media_recorder_handler_| gets destroyed and removing sinks
  // before the MediaStreamVideoTrack dtor, avoiding a DCHECK on a non-empty
  // callback list.
  media_recorder_handler_ = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerTest,
                         ValuesIn(kMediaRecorderTestParams));
class MediaRecorderHandlerTestForMp4
    : public TestWithParam<MediaRecorderTestParams>,
      public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerTestForMp4()
      : MediaRecorderHandlerFixture(GetParam().has_video,
                                    GetParam().has_audio) {
    scoped_feature_list_.InitAndDisableFeature(kMediaRecorderEnableMp4Muxer);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Array of valid combinations of video/audio/codecs for mp4.
static const MediaRecorderTestParams kMediaRecorderTestParamsForMp4[] = {
    {false, true, false, "video/mp4", "avc1", false},
    {false, true, false, "video/mp4", "avc1", false},
    {false, false, true, "audio/mp4", "mp4a.40.2", false},
    {false, true, true, "video/mp4", "avc1,mp4a.40.2", false},
    {false, true, true, "audio/mp4", "opus", false},
    {false, true, true, "video/mp4", "avc1,opus", false},
};

TEST_P(MediaRecorderHandlerTestForMp4,
       InitializeFailedWhenMP4MuxerFeatureDisabled) {
  if (!IsTargetAudioCodecSupported(GetParam().codecs)) {
    return;
  }

  // When feature is disabled, Initialize will fail.
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_FALSE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerTestForMp4,
                         ValuesIn(kMediaRecorderTestParamsForMp4));

class MediaRecorderHandlerIsSupportedTypeTestForMp4
    : public TestWithParam<bool>,
      public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerIsSupportedTypeTestForMp4()
      : MediaRecorderHandlerFixture(true, true) {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(kMediaRecorderEnableMp4Muxer);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kMediaRecorderEnableMp4Muxer);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that canSupportMimeType() works as expected, by sending supported
// combinations and unsupported ones.
TEST_P(MediaRecorderHandlerIsSupportedTypeTestForMp4,
       CanSupportMimeTypeForMp4) {
  // video types.
  const String good_mp4_video_mime_types[] = {"video/mp4"};
  const String bad_mp4_video_mime_types[] = {"video/MP4"};

  const String good_mp4_video_codecs[] = {"avc1", "avc1.420034", "vp9", "av01",
                                          "av01.2.19H.08.0.000.09.16.09.1"};
  const String bad_mp4_video_codecs[] = {"h264", "vp8",         "avc11",
                                         "aVc1", "avc1.123456", "av1"};

  const String good_mp4_video_codecs_non_proprietory[] = {
      "vp9", "av01", "av01.2.19H.08.0.000.09.16.09.1"};
  const String bad_mp4_video_codecs_non_proprietory[] = {
      "avc1", "h264", "vp8", "avc11", "aVc1", "avc1.123456", "av1"};

  // audio types.
  const String good_mp4_audio_mime_types[] = {"audio/mp4"};
  const String bad_mp4_audio_mime_types[] = {"AUDIO/mp4"};

  const String good_mp4_audio_codecs[] = {"mp4a.40.2, opus"};
  const String bad_mp4_audio_codecs[] = {"mp4a", "mp4a.40", "mP4a.40.2", "aac",
                                         "pcm"};

  const String good_mp4_audio_codecs_non_proprietory[] = {"opus"};
  const String bad_mp4_audio_codecs_non_proprietory[] = {
      "mp4a.40.2", "mp4a", "mp4a.40", "mP4a.40.2", "aac", "pcm"};

  if (GetParam()) {
    // mp4, enabled feature of kMediaRecorderEnableMp4Muxer.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    // success cases.
    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs) {
        if (!IsTargetAudioCodecSupported(codec)) {
          continue;
        }
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& video_codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(video_codec)) {
          continue;
        }
        for (const auto& audio_codec : good_mp4_audio_codecs) {
          if (!IsTargetAudioCodecSupported(audio_codec)) {
            continue;
          }
          String codecs = video_codec + "," + audio_codec;
          EXPECT_TRUE(
              media_recorder_handler_->CanSupportMimeType(type, codecs));

          String codecs2 = audio_codec + "," + video_codec;
          EXPECT_TRUE(
              media_recorder_handler_->CanSupportMimeType(type, codecs2));
        }
      }
    }

    // failure cases.
    for (const auto& type : bad_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : bad_mp4_video_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#else
    // success cases.
    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_video_codecs_non_proprietory) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs_non_proprietory) {
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& video_codec : good_mp4_video_codecs_non_proprietory) {
        if (!IsAv1CodecSupported(video_codec)) {
          continue;
        }
        for (const auto& audio_codec : good_mp4_audio_codecs_non_proprietory) {
          String codecs = video_codec + "," + audio_codec;
          EXPECT_TRUE(
              media_recorder_handler_->CanSupportMimeType(type, codecs));

          String codecs2 = audio_codec + "," + video_codec;
          EXPECT_TRUE(
              media_recorder_handler_->CanSupportMimeType(type, codecs2));
        }
      }
    }

    // failure cases.
    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : bad_mp4_video_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#endif

    // audio mime types.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    // success cases.
    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs) {
        if (!IsTargetAudioCodecSupported(codec)) {
          continue;
        }
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    // failure cases.
    for (const auto& type : bad_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : bad_mp4_audio_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& video_codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(video_codec)) {
          continue;
        }
        for (const auto& audio_codec : good_mp4_audio_codecs) {
          String codecs = video_codec + "," + audio_codec;
          EXPECT_FALSE(
              media_recorder_handler_->CanSupportMimeType(type, codecs));

          String codecs2 = audio_codec + "," + video_codec;
          EXPECT_FALSE(
              media_recorder_handler_->CanSupportMimeType(type, codecs2));
        }
      }
    }
#else
    // success cases.
    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs_non_proprietory) {
        EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }

    // failure cases.
    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : bad_mp4_audio_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#endif
  } else {
    // TODO(crbug.com/1072056): Once the feature, MediaRecorderEnableMp4Muxer,
    // is enabled, remove the below test.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#else
    for (const auto& type : good_mp4_video_mime_types) {
      for (const auto& codec : good_mp4_video_codecs) {
        if (!IsAv1CodecSupported(codec)) {
          continue;
        }
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#else
    for (const auto& type : good_mp4_audio_mime_types) {
      for (const auto& codec : good_mp4_audio_codecs) {
        EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(type, codec));
      }
    }
#endif
  }
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_P(MediaRecorderHandlerIsSupportedTypeTestForMp4,
       CanSupportAacCodecForWinNSku) {
  if (!GetParam()) {
    GTEST_SKIP();
  }

  if (!IsTargetAudioCodecSupported("mp4a.40.2")) {
    return;
  }

  {
    base::test::ScopedOSInfoOverride scoped_os_info_override(
        base::test::ScopedOSInfoOverride::Type::kWin11Home);
    EXPECT_TRUE(
        media_recorder_handler_->CanSupportMimeType("audio/mp4", "mp4a.40.2"));
  }

  {
    base::test::ScopedOSInfoOverride scoped_os_info_override(
        base::test::ScopedOSInfoOverride::Type::kWin11HomeN);
    EXPECT_FALSE(
        media_recorder_handler_->CanSupportMimeType("audio/mp4", "mp4a.40.2"));
  }
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS)

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaRecorderHandlerIsSupportedTypeTestForMp4,
    ValuesIn({/*MediaRecorderEnableMp4Muxer enabled=*/true,
              /*MediaRecorderEnableMp4Muxer disabled=*/false}));

class MediaRecorderHandlerAudioVideoTest : public testing::Test,
                                           public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerAudioVideoTest()
      : MediaRecorderHandlerFixture(/*has_video=*/true,
                                    /*has_audio=*/true) {}

  void FeedVideo() {
    media::Muxer::VideoParameters video_params(
        gfx::Size(), 1, media::VideoCodec::kVP9, gfx::ColorSpace());
    auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("video"));
    std::string alpha_data = "alpha";
    buffer->WritableSideData().alpha_data.assign(alpha_data.begin(),
                                                 alpha_data.end());
    buffer->set_is_key_frame(true);
    OnEncodedVideoForTesting(video_params, buffer, timestamp_);
    timestamp_ += base::Milliseconds(10);
  }

  void FeedAudio() {
    media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_PCM_LINEAR,
        media::ChannelLayoutConfig::Stereo(), kTestAudioSampleRate,
        kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000);
    auto buffer = media::DecoderBuffer::CopyFrom(base::as_byte_span("audio"));
    OnEncodedAudioForTesting(audio_params, buffer, timestamp_);
    timestamp_ += base::Milliseconds(10);
  }

  base::TimeTicks timestamp_ = base::TimeTicks::Now();
};

TEST_F(MediaRecorderHandlerAudioVideoTest, IgnoresStaleEncodedMediaOnRestart) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "video/webm", "vp9,opus",
      AudioTrackRecorder::BitrateMode::kVariable);
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);
  auto* audio_weak_cell = GetAudioCallbackInterface();
  auto* video_weak_cell = GetVideoCallbackInterface();
  EXPECT_TRUE(audio_weak_cell->Get());
  EXPECT_TRUE(video_weak_cell->Get());
  media_recorder_handler_->Stop();
  EXPECT_FALSE(audio_weak_cell->Get());
  EXPECT_FALSE(video_weak_cell->Get());

  // Start with a new session serial created by Stop.
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);
  EXPECT_TRUE(GetAudioCallbackInterface()->Get());
  EXPECT_TRUE(GetVideoCallbackInterface()->Get());
  media_recorder_handler_->Stop();
  media_recorder_handler_ = nullptr;
}

TEST_F(MediaRecorderHandlerAudioVideoTest, EmitsCachedAudioDataOnStop) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "video/webm", "vp9,opus",
      AudioTrackRecorder::BitrateMode::kVariable);
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);

  // Feed some encoded data into the recorder. Expect that data cached by the
  // muxer is emitted on the call to Stop.
  FeedVideo();
  FeedAudio();
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media_recorder_handler_->Stop();
  media_recorder_handler_ = nullptr;
  Mock::VerifyAndClearExpectations(recorder);
}

TEST_F(MediaRecorderHandlerAudioVideoTest, EmitsCachedVideoDataOnStop) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "video/webm", "vp9,opus",
      AudioTrackRecorder::BitrateMode::kVariable);
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);

  // Feed some encoded data into the recorder. Expect that data cached by the
  // muxer is emitted on the call to Stop.
  FeedAudio();
  FeedVideo();
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media_recorder_handler_->Stop();
  media_recorder_handler_ = nullptr;
  Mock::VerifyAndClearExpectations(recorder);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(MediaRecorderHandlerAudioVideoTest, CorrectH264LevelOnWrite) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "video/webm", "avc1.640022,opus",
      AudioTrackRecorder::BitrateMode::kVariable);

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(),
            "video/x-matroska;codecs=avc1.640022,opus");
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);

  // Feed some encoded data into the recorder. Expect that data cached by the
  // muxer is emitted on the call to Stop.
  FeedAudio();
  OnEncodedH264VideoForTesting(base::TimeTicks::Now());
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media_recorder_handler_->Stop();

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(),
            "video/x-matroska;codecs=avc1.64000d,opus");
  media_recorder_handler_ = nullptr;
  Mock::VerifyAndClearExpectations(recorder);
}
#endif

TEST_F(MediaRecorderHandlerAudioVideoTest,
       EmitsCachedAudioDataAfterVideoTrackEnded) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "video/webm", "vp9,opus",
      AudioTrackRecorder::BitrateMode::kVariable);
  media_recorder_handler_->Start(std::numeric_limits<int>::max(), "video/webm",
                                 0, 0);

  // Feed some encoded data into the recorder. Expect that data cached by the
  // muxer is emitted on the call to Stop.
  FeedVideo();
  registry_.test_stream()->VideoComponents()[0]->GetPlatformTrack()->Stop();
  FeedAudio();
  FeedAudio();
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media_recorder_handler_->Stop();
  media_recorder_handler_ = nullptr;
  Mock::VerifyAndClearExpectations(recorder);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

struct H264ProfileTestParams {
  const bool has_audio;
  const char* const mime_type;
  const char* const codecs;
};

static const H264ProfileTestParams kH264ProfileTestParams[] = {
    {false, "video/x-matroska", "avc1.42000c"},  // H264PROFILE_BASELINE
    {false, "video/x-matroska", "avc1.4d000c"},  // H264PROFILE_MAIN
    {false, "video/x-matroska", "avc1.64000c"},  // H264PROFILE_HIGH
    {false, "video/x-matroska", "avc1.640029"},
    {false, "video/x-matroska", "avc1.640034"},
    {true, "video/x-matroska", "avc1.64000c,pcm"},
    {false, "video/mp4", "avc1.42000c"},  // H264PROFILE_BASELINE
    {false, "video/mp4", "avc1.4d000c"},  // H264PROFILE_MAIN
    {false, "video/mp4", "avc1.64000c"},  // H264PROFILE_HIGH
    {false, "video/mp4", "avc1.640029"},
    {false, "video/mp4", "avc1.640034"},
};

class MediaRecorderHandlerH264ProfileTest
    : public TestWithParam<H264ProfileTestParams>,
      public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerH264ProfileTest()
      : MediaRecorderHandlerFixture(true, GetParam().has_audio) {
    scoped_feature_list_.InitAndEnableFeature(kMediaRecorderEnableMp4Muxer);
  }

  MediaRecorderHandlerH264ProfileTest(
      const MediaRecorderHandlerH264ProfileTest&) = delete;
  MediaRecorderHandlerH264ProfileTest& operator=(
      const MediaRecorderHandlerH264ProfileTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(MediaRecorderHandlerH264ProfileTest, ActualMimeType) {
  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));

  String actual_mime_type =
      String(GetParam().mime_type) + ";codecs=" + GetParam().codecs;

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(), actual_mime_type);

  media_recorder_handler_ = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerH264ProfileTest,
                         ValuesIn(kH264ProfileTestParams));

#if BUILDFLAG(IS_WIN)
class MediaRecorderHandlerWinAacCodecTest : public TestWithParam<unsigned int>,
                                            public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerWinAacCodecTest()
      : MediaRecorderHandlerFixture(false, true) {
    scoped_feature_list_.InitAndEnableFeature(kMediaRecorderEnableMp4Muxer);
  }

  MediaRecorderHandlerWinAacCodecTest(
      const MediaRecorderHandlerWinAacCodecTest&) = delete;
  MediaRecorderHandlerWinAacCodecTest& operator=(
      const MediaRecorderHandlerWinAacCodecTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(MediaRecorderHandlerWinAacCodecTest, AudioBitsPerSeconds) {
  const String codecs("mp4a.40.2");
  if (!IsTargetAudioCodecSupported(codecs)) {
    return;
  }

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type("audio/mp4");
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));
  media_recorder_handler_->Start(0, mime_type, GetParam(), 0);

  EXPECT_EQ(media::MFAudioEncoder::ClampAccCodecBitrate(GetParam()),
            recorder->audioBitsPerSecond());

  media_recorder_handler_->Stop();
  media_recorder_handler_ = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerWinAacCodecTest,
                         ValuesIn({5000u, 96000u, 128000u, 160000u, 192000u,
                                   256000u, 300000u}));

#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

struct MediaRecorderPassthroughTestParams {
  const char* mime_type;
  media::VideoCodec codec;
};

static const MediaRecorderPassthroughTestParams
    kMediaRecorderPassthroughTestParams[] = {
        {"video/webm;codecs=vp8", media::VideoCodec::kVP8},
        {"video/webm;codecs=vp9", media::VideoCodec::kVP9},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        {"video/x-matroska;codecs=avc1", media::VideoCodec::kH264},
#endif
        {"video/webm;codecs=av01", media::VideoCodec::kAV1},
};

class MediaRecorderHandlerPassthroughTest
    : public TestWithParam<MediaRecorderPassthroughTestParams>,
      public ScopedMockOverlayScrollbars {
 public:
  MediaRecorderHandlerPassthroughTest() {
    registry_.Init();
    video_source_ = registry_.AddVideoTrack(TestVideoTrackId());
    ON_CALL(*video_source_, SupportsEncodedOutput).WillByDefault(Return(true));
    media_recorder_handler_ = MakeGarbageCollected<MediaRecorderHandler>(
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        KeyFrameRequestProcessor::Configuration());
    EXPECT_FALSE(media_recorder_handler_->recording_);
  }

  MediaRecorderHandlerPassthroughTest(
      const MediaRecorderHandlerPassthroughTest&) = delete;
  MediaRecorderHandlerPassthroughTest& operator=(
      const MediaRecorderHandlerPassthroughTest&) = delete;

  ~MediaRecorderHandlerPassthroughTest() override {
    registry_.reset();
    media_recorder_handler_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void OnVideoFrameForTesting(scoped_refptr<EncodedVideoFrame> frame) {
    media_recorder_handler_->OnEncodedVideoFrameForTesting(
        std::move(frame), base::TimeTicks::Now());
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  MockMediaStreamRegistry registry_;
  raw_ptr<MockMediaStreamVideoSource, DanglingUntriaged> video_source_ =
      nullptr;
  Persistent<MediaRecorderHandler> media_recorder_handler_;
};

TEST_P(MediaRecorderHandlerPassthroughTest, PassesThrough) {
  // Setup the mock video source to allow for passthrough recording.
  EXPECT_CALL(*video_source_, OnEncodedSinkEnabled);
  EXPECT_CALL(*video_source_, OnEncodedSinkDisabled);

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "", "",
      AudioTrackRecorder::BitrateMode::kVariable);
  media_recorder_handler_->Start(0, "", 0, 0);

  const size_t kFrameSize = 42;
  auto frame = FakeEncodedVideoFrame::Builder()
                   .WithKeyFrame(true)
                   .WithCodec(GetParam().codec)
                   .WithData(std::string(kFrameSize, 'P'))
                   .BuildRefPtr();
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(SizeIs(Ge(kFrameSize)), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    OnVideoFrameForTesting(frame);
    run_loop.Run();
  }

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(),
            String(GetParam().mime_type));
  Mock::VerifyAndClearExpectations(recorder);

  media_recorder_handler_->Stop();
}

TEST_F(MediaRecorderHandlerPassthroughTest, ErrorsOutOnCodecSwitch) {
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "", "",
      AudioTrackRecorder::BitrateMode::kVariable));
  EXPECT_TRUE(media_recorder_handler_->Start(0, "", 0, 0));

  // NOTE, Asan: the prototype of WriteData which has a const char* as data
  // ptr plays badly with gmock which tries to interpret it as a null-terminated
  // string. However, it points to binary data which causes gmock to overrun the
  // bounds of buffers and this manifests as an ASAN crash.
  // The expectation here works around this issue.
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));

  EXPECT_CALL(*recorder, OnError).WillOnce(InvokeWithoutArgs([&]() {
    // Simulate MediaRecorder behavior which is to Stop() the handler on error.
    media_recorder_handler_->Stop();
  }));
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::VideoCodec::kVP8)
                             .WithData(std::string("vp8 frame"))
                             .BuildRefPtr());
  // Switch to VP9 frames. This is expected to cause the call to OnError
  // above.
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::VideoCodec::kVP9)
                             .WithData(std::string("vp9 frame"))
                             .BuildRefPtr());
  // Send one more frame to verify that continued frame of different codec
  // transfer doesn't crash the media recorder.
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::VideoCodec::kVP8)
                             .WithData(std::string("vp8 frame"))
                             .BuildRefPtr());
  platform_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(recorder);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerPassthroughTest,
                         ValuesIn(kMediaRecorderPassthroughTestParams));

}  // namespace blink
