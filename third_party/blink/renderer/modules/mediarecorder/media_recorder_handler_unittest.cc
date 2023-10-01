// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/modules/mediarecorder/fake_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
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
using ::testing::TestWithParam;
using ::testing::ValuesIn;

#if BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_MAC) && BUILDFLAG(USE_PROPRIETARY_CODECS))
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
    {true, true, false, "video/mp4", "avc1", false},
    {true, false, true, "audio/mp4", "aac", false},
#endif
    {true, false, true, "audio/webm", "opus", true},
    {true, false, true, "audio/webm", "", true},  // Should default to opus.
    {true, false, true, "audio/webm", "pcm", true},
    {true, true, true, "video/webm", "vp9,opus", true},
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

  MOCK_METHOD(void,
              WriteData,
              (const void*, size_t, bool, double, ErrorEvent*));
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

  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame) {
    media_recorder_handler_->OnVideoFrameForTesting(std::move(frame),
                                                    base::TimeTicks::Now());
  }

  void OnEncodedVideoForTesting(const media::Muxer::VideoParameters& params,
                                std::string encoded_data,
                                std::string encoded_alpha,
                                base::TimeTicks timestamp,
                                bool is_key_frame) {
    media_recorder_handler_->OnEncodedVideo(
        params, std::move(encoded_data), std::move(encoded_alpha),
        absl::nullopt, timestamp, is_key_frame);
  }

  void OnEncodedAudioForTesting(const media::AudioParameters& params,
                                std::string encoded_data,
                                base::TimeTicks timestamp) {
    media_recorder_handler_->OnEncodedAudio(params, std::move(encoded_data),
                                            absl::nullopt, timestamp);
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
    static_cast<media::WebmMuxer*>(media_recorder_handler_->muxer_.get())
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

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  MockMediaStreamRegistry registry_;
  bool has_video_;
  bool has_audio_;
  Persistent<MediaRecorderHandler> media_recorder_handler_;
  media::SineWaveAudioSource audio_source_;
  raw_ptr<MockMediaStreamVideoSource, ExperimentalRenderer> video_source_ =
      nullptr;
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
#if !BUILDFLAG(RTC_USE_H264)
    // Test requires OpenH264 encoder. It can't use the VEA encoder.
    if (std::string(GetParam().codecs) == "avc1") {
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
      mime_type_video, example_good_codecs_8));

  const String example_unsupported_codecs_2("vorbis");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_unsupported_codecs_2));

  if (!GetParam().mp4_enabled) {
    // mp4, disabled feature of kMediaRecorderEnableMp4Muxer.
    const String mime_type_video_1("video/mp4");
    const String mime_type_video_uppercase_1("video/MP4");
    const String example_good_codecs_mp4_1("h264");
    const String example_good_codecs_mp4_2("avc1");
    const String example_good_codecs_mp4_3("aVC1");
    const String example_good_codecs_mp4_4("h264,aac");
    const String example_good_codecs_mp4_5("avc1,aac");

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(mime_type_video_1,
                                                             String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_uppercase_1, String()));

    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_1));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_2));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_3));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_4));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_5));

    const String example_bad_codecs_mp4_1("vp8");
    const String example_bad_codecs_mp4_2("vp9");
    const String example_bad_codecs_mp4_3("opus");
    const String example_bad_codecs_mp4_4("pcm");
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_1));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_2));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_3));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_4));
#else
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_uppercase_1, String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_1));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_2));
#endif

    const String mime_type_audio_mp4_1("audio/mp4");
    const String example_good_codecs_mp4_10("aac");
    const String example_good_codecs_mp4_11("aAc");
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_10));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_11));

    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_bad_codecs_mp4_3));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_bad_codecs_mp4_4));
#else
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_10));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_11));
#endif
  } else {
    // mp4, enabled feature of kMediaRecorderEnableMp4Muxer.
    const String mime_type_video_1("video/mp4");
    const String mime_type_video_uppercase_1("video/MP4");
    const String example_good_codecs_mp4_1("h264");
    const String example_good_codecs_mp4_2("avc1");
    const String example_good_codecs_mp4_3("aVC1");
    const String example_good_codecs_mp4_4("h264,aac");
    const String example_good_codecs_mp4_5("avc1,aac");

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(mime_type_video_1,
                                                            String()));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_uppercase_1, String()));

    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_1));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_2));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_3));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_4));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_5));

    const String example_bad_codecs_mp4_1("vp8");
    const String example_bad_codecs_mp4_2("vp9");
    const String example_bad_codecs_mp4_3("opus");
    const String example_bad_codecs_mp4_4("pcm");
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_1));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_2));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_3));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_bad_codecs_mp4_4));
#else
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_uppercase_1, String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_1));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_video_1, example_good_codecs_mp4_2));
#endif

    const String mime_type_audio_mp4_1("audio/mp4");
    const String example_good_codecs_mp4_10("aac");
    const String example_good_codecs_mp4_11("aAc");
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, String()));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_10));
    EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_11));

    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_bad_codecs_mp4_3));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_bad_codecs_mp4_4));
#else
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, String()));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_10));
    EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
        mime_type_audio_mp4_1, example_good_codecs_mp4_11));
#endif
  }
}

// Checks that it uses the specified bitrate mode.
TEST_P(MediaRecorderHandlerTest, SupportsBitrateMode) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

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

#if !defined(HAS_AAC_ENCODER)
  if (EqualIgnoringASCIICase(codecs, "aac")) {
    return;
  }
#endif

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
  // Video-only test.
  if (GetParam().has_audio || !IsCodecSupported() ||
      !IsStreamWriteSupported()) {
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
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
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
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
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
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    if (GetParam().encoder_supports_alpha) {
      EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    }
    OnVideoFrameForTesting(alpha_frame);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(recorder);
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

  const size_t kEncodedSizeThreshold = 24;
  {
    base::RunLoop run_loop;
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i)
      OnAudioBusForTesting(*audio_bus1);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(recorder);

  {
    base::RunLoop run_loop;
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i)
      OnAudioBusForTesting(*audio_bus2);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(recorder);

  media_recorder_handler_->Stop();
}

// Starts up recording and forces a WebmMuxer's libwebm error.
TEST_P(MediaRecorderHandlerTest, WebmMuxerErrorWhileEncoding) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio || !IsCodecSupported() ||
      !IsStreamWriteSupported()) {
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
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _, _))
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
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs,
      AudioTrackRecorder::BitrateMode::kVariable));

  StringBuilder actual_mime_type;
  actual_mime_type.Append(GetParam().mime_type);
  actual_mime_type.Append(";codecs=");
  if (strlen(GetParam().codecs) != 0u) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (EqualIgnoringASCIICase(GetParam().codecs, "aac")) {
      actual_mime_type.Append("m4a.40.2");
    } else {
      actual_mime_type.Append(GetParam().codecs);
    }
#else
    actual_mime_type.Append(GetParam().codecs);
#endif

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
  if (GetParam().has_audio || !IsStreamWriteSupported()) {
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

  Mock::VerifyAndClearExpectations(recorder);
  media_recorder_handler_->Pause();

  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media::Muxer::VideoParameters params(gfx::Size(), 1, media::VideoCodec::kVP9,
                                       gfx::ColorSpace());
  OnEncodedVideoForTesting(params, "vp9 frame", "", base::TimeTicks::Now(),
                           true);

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
  OnEncodedVideoForTesting(params, "vp9 frame", "", base::TimeTicks::Now(),
                           true);

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
    {false, true, false, "video/mp4", "h264", false},
    {false, false, true, "audio/mp4", "aac", false},
    {false, true, true, "video/mp4", "h264,aac", false},
};

TEST_P(MediaRecorderHandlerTestForMp4,
       InitializeFailedWhenMP4MuxerFeatureDisabled) {
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

class MediaRecorderHandlerAudioVideoTest : public testing::Test,
                                           public MediaRecorderHandlerFixture {
 public:
  MediaRecorderHandlerAudioVideoTest()
      : MediaRecorderHandlerFixture(/*has_video=*/true,
                                    /*has_audio=*/true) {}

  void FeedVideo() {
    media::Muxer::VideoParameters video_params(
        gfx::Size(), 1, media::VideoCodec::kVP9, gfx::ColorSpace());
    OnEncodedVideoForTesting(video_params, "video", "alpha", timestamp_, true);
    timestamp_ += base::Milliseconds(10);
  }

  void FeedAudio() {
    media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_PCM_LINEAR,
        media::ChannelLayoutConfig::Stereo(), kTestAudioSampleRate,
        kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000);
    OnEncodedAudioForTesting(audio_params, "audio", timestamp_);
    timestamp_ += base::Milliseconds(10);
  }

  base::TimeTicks timestamp_ = base::TimeTicks::Now();
};

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
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {false, "video/mp4", "avc1.42000c"},  // H264PROFILE_BASELINE
    {false, "video/mp4", "avc1.4d000c"},  // H264PROFILE_MAIN
    {false, "video/mp4", "avc1.64000c"},  // H264PROFILE_HIGH
    {false, "video/mp4", "avc1.640029"},
    {false, "video/mp4", "avc1.640034"},
#endif
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

#endif

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

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  MockMediaStreamRegistry registry_;
  raw_ptr<MockMediaStreamVideoSource, ExperimentalRenderer> video_source_ =
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
    EXPECT_CALL(*recorder, WriteData(_, Ge(kFrameSize), _, _, _))
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
