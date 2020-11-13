// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/fake_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/gpu_memory_buffer.h"

using media::VideoFrame;
using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace blink {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

const VideoTrackRecorder::CodecId kTrackRecorderTestCodec[] = {
    VideoTrackRecorder::CodecId::VP8, VideoTrackRecorder::CodecId::VP9
#if BUILDFLAG(RTC_USE_H264)
    ,
    VideoTrackRecorder::CodecId::H264
#endif
};
const gfx::Size kTrackRecorderTestSize[] = {
    gfx::Size(kVEAEncoderMinResolutionWidth / 2,
              kVEAEncoderMinResolutionHeight / 2),
    gfx::Size(kVEAEncoderMinResolutionWidth, kVEAEncoderMinResolutionHeight)};
const media::VideoFrame::StorageType kStorageTypeToTest[] = {
    media::VideoFrame::STORAGE_OWNED_MEMORY,
    media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER};
static const int kTrackRecorderTestSizeDiff = 20;

media::VideoCodec MediaVideoCodecFromCodecId(VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::VP8:
      return media::kCodecVP8;
    case VideoTrackRecorder::CodecId::VP9:
      return media::kCodecVP9;
#if BUILDFLAG(RTC_USE_H264)
    case VideoTrackRecorder::CodecId::H264:
      return media::kCodecH264;
#endif
    default:
      return media::kUnknownVideoCodec;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::kUnknownVideoCodec;
}

class MockTestingPlatform : public IOTaskRunnerTestingPlatformSupport {
 public:
  MockTestingPlatform() = default;
  ~MockTestingPlatform() override = default;

  MOCK_METHOD0(GetGpuFactories, media::GpuVideoAcceleratorFactories*());
};

class VideoTrackRecorderTest
    : public TestWithParam<testing::tuple<VideoTrackRecorder::CodecId,
                                          gfx::Size,
                                          bool,
                                          media::VideoFrame::StorageType>> {
 public:
  VideoTrackRecorderTest() : mock_source_(new MockMediaStreamVideoSource()) {
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/);
    source_->SetPlatformSource(base::WrapUnique(mock_source_));
    EXPECT_CALL(*mock_source_, OnRequestRefreshFrame())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*mock_source_, OnCapturingLinkSecured(_))
        .Times(testing::AnyNumber());
    component_ = MakeGarbageCollected<MediaStreamComponent>(source_);

    track_ = new MediaStreamVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true /* enabled */);
    component_->SetPlatformTrack(base::WrapUnique(track_));

    // Paranoia checks.
    EXPECT_EQ(component_->Source()->GetPlatformSource(),
              source_->GetPlatformSource());
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());

    ON_CALL(*platform_, GetGpuFactories()).WillByDefault(Return(nullptr));
  }

  ~VideoTrackRecorderTest() {
    component_ = nullptr;
    source_ = nullptr;
    video_track_recorder_.reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  void InitializeRecorder(VideoTrackRecorder::CodecId codec_id) {
    InitializeRecorder(VideoTrackRecorder::CodecProfile(codec_id));
  }

  void InitializeRecorder(VideoTrackRecorder::CodecProfile codec_profile) {
    video_track_recorder_ = std::make_unique<VideoTrackRecorderImpl>(
        codec_profile, WebMediaStreamTrack(component_.Get()),
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&VideoTrackRecorderTest::OnEncodedVideo,
                                     CrossThreadUnretained(this))),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            &VideoTrackRecorderTest::OnSourceReadyStateEnded,
            CrossThreadUnretained(this))),
        0 /* bits_per_second */,
        scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  MOCK_METHOD0(OnSourceReadyStateEnded, void());

  MOCK_METHOD5(OnEncodedVideo,
               void(const media::WebmMuxer::VideoParameters& params,
                    std::string encoded_data,
                    std::string encoded_alpha,
                    base::TimeTicks timestamp,
                    bool keyframe));

  void Encode(scoped_refptr<VideoFrame> frame, base::TimeTicks capture_time) {
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());
    video_track_recorder_->OnVideoFrameForTesting(std::move(frame),
                                                  capture_time);
  }

  void OnError() { video_track_recorder_->OnError(); }

  bool CanEncodeAlphaChannel() {
    return video_track_recorder_->encoder_->CanEncodeAlphaChannel();
  }

  bool HasEncoderInstance() { return video_track_recorder_->encoder_.get(); }

  uint32_t NumFramesInEncode() {
    return video_track_recorder_->encoder_->num_frames_in_encode_->count();
  }

  ScopedTestingPlatformSupport<MockTestingPlatform> platform_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |source_|, |track_| by |component_|.
  MockMediaStreamVideoSource* mock_source_;
  Persistent<MediaStreamSource> source_;
  MediaStreamVideoTrack* track_;
  Persistent<MediaStreamComponent> component_;

  std::unique_ptr<VideoTrackRecorderImpl> video_track_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoTrackRecorderTest);
};

// Construct and destruct all objects, in particular |video_track_recorder_| and
// its inner object(s). This is a non trivial sequence.
TEST_P(VideoTrackRecorderTest, ConstructAndDestruct) {
  InitializeRecorder(testing::get<0>(GetParam()));
}

TEST_F(VideoTrackRecorderTest, RelaysReadyStateEnded) {
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);
  EXPECT_CALL(*this, OnSourceReadyStateEnded);
  mock_source_->StopSource();
}

// Creates the encoder and encodes 2 frames of the same size; the encoder
// should be initialised and produce a keyframe, then a non-keyframe. Finally
// a frame of larger size is sent and is expected to be encoded as a keyframe.
// If |encode_alpha_channel| is enabled, encoder is expected to return a
// second output with encoded alpha data.
TEST_P(VideoTrackRecorderTest, VideoEncoding) {
  InitializeRecorder(testing::get<0>(GetParam()));

  const bool encode_alpha_channel = testing::get<2>(GetParam());
  // |frame_size| cannot be arbitrarily small, should be reasonable.
  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const media::VideoFrame::StorageType storage_type =
      testing::get<3>(GetParam());

  // We don't support alpha channel with GpuMemoryBuffer frames.
  if (storage_type == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      encode_alpha_channel) {
    return;
  }

  auto create_test_frame =
      [](media::VideoFrame::StorageType storage_type,
         const gfx::Size& frame_size,
         bool encode_alpha_channel) -> scoped_refptr<VideoFrame> {
    scoped_refptr<VideoFrame> video_frame;
    switch (storage_type) {
      case media::VideoFrame::STORAGE_OWNED_MEMORY:
        video_frame = encode_alpha_channel
                          ? VideoFrame::CreateTransparentFrame(frame_size)
                          : VideoFrame::CreateBlackFrame(frame_size);
        break;
      case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER: {
        video_frame = CreateTestFrame(frame_size, gfx::Rect(frame_size),
                                      frame_size, storage_type);
        // Create a black NV12 frame.
        auto* gmb = video_frame->GetGpuMemoryBuffer();
        gmb->Map();
        const uint8_t kBlackY = 0x00;
        const uint8_t kBlackUV = 0x80;
        memset(static_cast<uint8_t*>(gmb->memory(0)), kBlackY,
               gmb->stride(0) * frame_size.height());
        memset(static_cast<uint8_t*>(gmb->memory(1)), kBlackUV,
               gmb->stride(1) * (frame_size.height() / 2));
        gmb->Unmap();
        break;
      }
      default:
        DLOG(ERROR) << "Unexpected storage type";
    }
    return video_frame;
  };

  const scoped_refptr<VideoFrame> video_frame =
      create_test_frame(storage_type, frame_size, encode_alpha_channel);
  if (!video_frame)
    ASSERT_TRUE(!!video_frame);

  const double kFrameRate = 60.0f;
  video_frame->metadata()->frame_rate = kFrameRate;

  InSequence s;
  const base::TimeTicks timeticks_now = base::TimeTicks::Now();
  base::StringPiece first_frame_encoded_data;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, timeticks_now, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&first_frame_encoded_data),
                      SaveArg<2>(&first_frame_encoded_alpha)));
  Encode(video_frame, timeticks_now);

  // Send another Video Frame.
  const base::TimeTicks timeticks_later = base::TimeTicks::Now();
  base::StringPiece second_frame_encoded_data;
  base::StringPiece second_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, timeticks_later, false))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&second_frame_encoded_data),
                      SaveArg<2>(&second_frame_encoded_alpha)));
  Encode(video_frame, timeticks_later);

  // Send another Video Frame and expect only an OnEncodedVideo() callback.
  const gfx::Size frame_size2(frame_size.width() + kTrackRecorderTestSizeDiff,
                              frame_size.height());
  const scoped_refptr<VideoFrame> video_frame2 =
      create_test_frame(storage_type, frame_size2, encode_alpha_channel);

  base::RunLoop run_loop;

  base::StringPiece third_frame_encoded_data;
  base::StringPiece third_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&third_frame_encoded_data),
                      SaveArg<2>(&third_frame_encoded_alpha),
                      RunClosure(run_loop.QuitClosure())));
  Encode(video_frame2, base::TimeTicks::Now());

  run_loop.Run();

  const size_t kEncodedSizeThreshold = 14;
  EXPECT_GE(first_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(second_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(third_frame_encoded_data.size(), kEncodedSizeThreshold);

  // We only support NV12 with GpuMemoryBuffer video frame.
  if (storage_type != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      encode_alpha_channel && CanEncodeAlphaChannel()) {
    EXPECT_GE(first_frame_encoded_alpha.size(), kEncodedSizeThreshold);
    EXPECT_GE(second_frame_encoded_alpha.size(), kEncodedSizeThreshold);
    EXPECT_GE(third_frame_encoded_alpha.size(), kEncodedSizeThreshold);
  } else {
    const size_t kEmptySize = 0;
    EXPECT_EQ(first_frame_encoded_alpha.size(), kEmptySize);
    EXPECT_EQ(second_frame_encoded_alpha.size(), kEmptySize);
    EXPECT_EQ(third_frame_encoded_alpha.size(), kEmptySize);
  }

  Mock::VerifyAndClearExpectations(this);
}

// Inserts a frame which has different coded size than the visible rect and
// expects encode to be completed without raising any sanitizer flags.
TEST_P(VideoTrackRecorderTest, EncodeFrameWithPaddedCodedSize) {
  InitializeRecorder(testing::get<0>(GetParam()));

  const gfx::Size& frame_size = testing::get<1>(GetParam());
  const size_t kCodedSizePadding = 16;
  const media::VideoFrame::StorageType storage_type =
      testing::get<3>(GetParam());
  const gfx::Size padded_size(frame_size.width() + kCodedSizePadding,
                              frame_size.height());
  scoped_refptr<VideoFrame> video_frame;
  switch (storage_type) {
    case media::VideoFrame::STORAGE_OWNED_MEMORY:
      video_frame = VideoFrame::CreateZeroInitializedFrame(
          media::PIXEL_FORMAT_I420, padded_size, gfx::Rect(frame_size),
          frame_size, base::TimeDelta());
      break;
    case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      video_frame = CreateTestFrame(padded_size, gfx::Rect(frame_size),
                                    frame_size, storage_type);
      break;
    default:
      NOTREACHED() << "Unexpected storage type";
  }

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Inserts an opaque frame followed by two transparent frames and expects the
// newly introduced transparent frame to force keyframe output.
TEST_F(VideoTrackRecorderTest, ForceKeyframeOnAlphaSwitch) {
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<VideoFrame> opaque_frame =
      VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(SaveArg<2>(&first_frame_encoded_alpha));
  Encode(opaque_frame, base::TimeTicks::Now());

  const scoped_refptr<VideoFrame> alpha_frame =
      VideoFrame::CreateTransparentFrame(frame_size);
  base::StringPiece second_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(SaveArg<2>(&second_frame_encoded_alpha));
  Encode(alpha_frame, base::TimeTicks::Now());

  base::RunLoop run_loop;
  base::StringPiece third_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, false))
      .Times(1)
      .WillOnce(DoAll(SaveArg<2>(&third_frame_encoded_alpha),
                      RunClosure(run_loop.QuitClosure())));
  Encode(alpha_frame, base::TimeTicks::Now());
  run_loop.Run();

  const size_t kEmptySize = 0;
  EXPECT_EQ(first_frame_encoded_alpha.size(), kEmptySize);
  EXPECT_GT(second_frame_encoded_alpha.size(), kEmptySize);
  EXPECT_GT(third_frame_encoded_alpha.size(), kEmptySize);

  Mock::VerifyAndClearExpectations(this);
}

// Inserts an OnError() call between sent frames.
TEST_F(VideoTrackRecorderTest, HandlesOnError) {
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true)).Times(1);
  Encode(video_frame, base::TimeTicks::Now());

  EXPECT_TRUE(HasEncoderInstance());
  OnError();
  EXPECT_FALSE(HasEncoderInstance());

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

// Inserts a frame for encode and makes sure that it is released properly and
// NumFramesInEncode() is updated.
TEST_F(VideoTrackRecorderTest, ReleasesFrame) {
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);

  base::RunLoop run_loop;
  bool frame_is_destroyed = false;
  auto set_to_true = [](bool* b) { *b = true; };
  video_frame->AddDestructionObserver(
      base::BindOnce(set_to_true, &frame_is_destroyed));
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitWhenIdleClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  video_frame = nullptr;
  run_loop.Run();
  EXPECT_EQ(0u, NumFramesInEncode());
  EXPECT_TRUE(frame_is_destroyed);

  Mock::VerifyAndClearExpectations(this);
}

// Waits for HW encoder support to be enumerated before setting up and
// performing an encode.
TEST_F(VideoTrackRecorderTest, WaitForEncoderSupport) {
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);
  EXPECT_CALL(*platform_, GetGpuFactories())
      .WillRepeatedly(Return(&mock_gpu_factories));

  EXPECT_CALL(mock_gpu_factories, NotifyEncoderSupportKnown(_))
      .WillOnce(base::test::RunOnceClosure<0>());
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .WillOnce(RunClosure(run_loop.QuitWhenIdleClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop.Run();
}

TEST_F(VideoTrackRecorderTest, RequiredRefreshRate) {
  // |RequestRefreshFrame| will be called first by |AddSink| and the second time
  // by the refresh timer using the required min fps.
  EXPECT_CALL(*mock_source_, OnRequestRefreshFrame).Times(2);

  track_->SetIsScreencastForTesting(true);
  InitializeRecorder(VideoTrackRecorder::CodecId::VP8);

  EXPECT_EQ(video_track_recorder_->GetRequiredMinFramesPerSec(), 1);

  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderTest,
                         ::testing::Combine(ValuesIn(kTrackRecorderTestCodec),
                                            ValuesIn(kTrackRecorderTestSize),
                                            ::testing::Bool(),
                                            ValuesIn(kStorageTypeToTest)));

class VideoTrackRecorderPassthroughTest
    : public TestWithParam<VideoTrackRecorder::CodecId> {
 public:
  VideoTrackRecorderPassthroughTest()
      : mock_source_(new MockMediaStreamVideoSource()) {
    ON_CALL(*mock_source_, SupportsEncodedOutput).WillByDefault(Return(true));
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/);
    source_->SetPlatformSource(base::WrapUnique(mock_source_));
    component_ = MakeGarbageCollected<MediaStreamComponent>(source_);

    track_ = new MediaStreamVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true /* enabled */);
    component_->SetPlatformTrack(base::WrapUnique(track_));

    // Paranoia checks.
    EXPECT_EQ(component_->Source()->GetPlatformSource(),
              source_->GetPlatformSource());
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());
  }

  ~VideoTrackRecorderPassthroughTest() {
    component_ = nullptr;
    source_ = nullptr;
    video_track_recorder_.reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  void InitializeRecorder() {
    video_track_recorder_ = std::make_unique<VideoTrackRecorderPassthrough>(
        WebMediaStreamTrack(component_.Get()),
        ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
            &VideoTrackRecorderPassthroughTest::OnEncodedVideo,
            CrossThreadUnretained(this))),
        ConvertToBaseOnceCallback(CrossThreadBindOnce([] {})),
        scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  MOCK_METHOD5(OnEncodedVideo,
               void(const media::WebmMuxer::VideoParameters& params,
                    std::string encoded_data,
                    std::string encoded_alpha,
                    base::TimeTicks timestamp,
                    bool is_key_frame));

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |source_|, |track_| by |component_|.
  MockMediaStreamVideoSource* mock_source_;
  Persistent<MediaStreamSource> source_;
  MediaStreamVideoTrack* track_;
  Persistent<MediaStreamComponent> component_;

  std::unique_ptr<VideoTrackRecorderPassthrough> video_track_recorder_;
};

scoped_refptr<FakeEncodedVideoFrame> CreateFrame(
    bool is_key_frame,
    VideoTrackRecorder::CodecId codec) {
  return FakeEncodedVideoFrame::Builder()
      .WithKeyFrame(is_key_frame)
      .WithData("abc")
      .WithCodec(MediaVideoCodecFromCodecId(codec))
      .BuildRefPtr();
}

TEST_F(VideoTrackRecorderPassthroughTest, RequestsAndFinishesEncodedOutput) {
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled);
  InitializeRecorder();
}

void DoNothing() {}

// Matcher for checking codec type
MATCHER_P(IsSameCodec, codec, "") {
  return arg.codec == MediaVideoCodecFromCodecId(codec);
}

TEST_P(VideoTrackRecorderPassthroughTest, HandlesFrames) {
  ON_CALL(*mock_source_, OnEncodedSinkEnabled).WillByDefault(DoNothing);
  ON_CALL(*mock_source_, OnEncodedSinkDisabled).WillByDefault(DoNothing);
  InitializeRecorder();

  // Frame 1 (keyframe)
  auto frame = CreateFrame(/*is_key_frame=*/true, GetParam());
  std::string encoded_data;
  EXPECT_CALL(*this, OnEncodedVideo(IsSameCodec(GetParam()), _, _, _, true))
      .WillOnce(DoAll(SaveArg<1>(&encoded_data)));
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  EXPECT_EQ(encoded_data, "abc");

  // Frame 2 (deltaframe)
  frame = CreateFrame(/*is_key_frame=*/false, GetParam());
  EXPECT_CALL(*this, OnEncodedVideo(IsSameCodec(GetParam()), _, _, _, false));
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
}

TEST_F(VideoTrackRecorderPassthroughTest, DoesntForwardDeltaFrameFirst) {
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled);
  InitializeRecorder();
  Mock::VerifyAndClearExpectations(mock_source_);

  // Frame 1 (deltaframe) - not forwarded
  auto frame =
      CreateFrame(/*is_key_frame=*/false, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, false)).Times(0);
  // We already requested a keyframe when starting the recorder, so expect
  // no keyframe request now
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled).Times(0);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled).Times(0);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(mock_source_);

  // Frame 2 (keyframe)
  frame = CreateFrame(/*is_key_frame=*/true, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true));
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);

  // Frame 3 (deltaframe) - forwarded
  base::RunLoop run_loop;
  frame = CreateFrame(/*is_key_frame=*/false, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  run_loop.Run();
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled);
}

TEST_F(VideoTrackRecorderPassthroughTest, PausesAndResumes) {
  InitializeRecorder();
  // Frame 1 (keyframe)
  auto frame =
      CreateFrame(/*is_key_frame=*/true, VideoTrackRecorder::CodecId::VP9);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  video_track_recorder_->Pause();

  // Expect no frame throughput now.
  frame = CreateFrame(/*is_key_frame=*/false, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo).Times(0);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);

  // Resume - expect keyframe request
  Mock::VerifyAndClearExpectations(mock_source_);
  // Expect no callback registration, but expect a keyframe.
  EXPECT_CALL(*mock_source_, OnEncodedSinkEnabled).Times(0);
  EXPECT_CALL(*mock_source_, OnEncodedSinkDisabled).Times(0);
  EXPECT_CALL(*mock_source_, OnRequestRefreshFrame);
  video_track_recorder_->Resume();
  Mock::VerifyAndClearExpectations(mock_source_);

  // Expect no transfer from deltaframe and transfer of keyframe
  frame = CreateFrame(/*is_key_frame=*/false, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo).Times(0);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);

  frame = CreateFrame(/*is_key_frame=*/true, VideoTrackRecorder::CodecId::VP9);
  EXPECT_CALL(*this, OnEncodedVideo);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderPassthroughTest,
                         ValuesIn(kTrackRecorderTestCodec));

class CodecEnumeratorTest : public ::testing::Test {
 public:
  using CodecEnumerator = VideoTrackRecorder::CodecEnumerator;
  using CodecId = VideoTrackRecorder::CodecId;

  CodecEnumeratorTest() = default;
  ~CodecEnumeratorTest() override = default;

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp8Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    profiles.emplace_back(media::VP8PROFILE_ANY, gfx::Size(1920, 1080), 30, 1);
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp9Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    profiles.emplace_back(media::VP9PROFILE_PROFILE1, gfx::Size(1920, 1080), 60,
                          1);
    profiles.emplace_back(media::VP9PROFILE_PROFILE2, gfx::Size(1920, 1080), 30,
                          1);
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp8Vp9Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles =
        MakeVp8Profiles();
    media::VideoEncodeAccelerator::SupportedProfiles vp9_profiles =
        MakeVp9Profiles();
    profiles.insert(profiles.end(), vp9_profiles.begin(), vp9_profiles.end());
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeH264Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    profiles.emplace_back(media::H264PROFILE_BASELINE, gfx::Size(1920, 1080),
                          24, 1);
    profiles.emplace_back(media::H264PROFILE_MAIN, gfx::Size(1920, 1080), 30,
                          1);
    profiles.emplace_back(media::H264PROFILE_HIGH, gfx::Size(1920, 1080), 60,
                          1);
    return profiles;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodecEnumeratorTest);
};

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdDefault) {
  // Empty supported profiles.
  const CodecEnumerator emulator(
      (media::VideoEncodeAccelerator::SupportedProfiles()));
  EXPECT_EQ(CodecId::VP8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8) {
  const CodecEnumerator emulator(MakeVp8Profiles());
  EXPECT_EQ(CodecId::VP8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(CodecId::VP9, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8Vp9) {
  const CodecEnumerator emulator(MakeVp8Vp9Profiles());
  EXPECT_EQ(CodecId::VP8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::VP9);
  EXPECT_EQ(2u, profiles.size());
  EXPECT_EQ(media::VP9PROFILE_PROFILE1, profiles[0].profile);
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, profiles[1].profile);
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::VP8);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(media::VP9PROFILE_PROFILE1,
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::VP9));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(media::VIDEO_CODEC_PROFILE_UNKNOWN,
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::VP8));
}

#if BUILDFLAG(RTC_USE_H264)
TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(media::H264PROFILE_HIGH,
            emulator.FindSupportedVideoCodecProfile(CodecId::H264,
                                                    media::H264PROFILE_HIGH));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileNoProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(media::VIDEO_CODEC_PROFILE_UNKNOWN,
            emulator.FindSupportedVideoCodecProfile(
                CodecId::H264, media::H264PROFILE_HIGH422PROFILE));
}
#endif

}  // namespace blink
