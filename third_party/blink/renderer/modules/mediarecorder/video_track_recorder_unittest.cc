// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/gpu_memory_buffer.h"

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
namespace {

// Specifies frame type for test.
enum class TestFrameType {
  kNv12GpuMemoryBuffer,  // Implies media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER
  kNv12Software,         // Implies media::VideoFrame::STORAGE_OWNED_MEMORY
  kI420                  // Implies media::VideoFrame::STORAGE_OWNED_MEMORY
};

const TestFrameType kTestFrameTypes[] = {TestFrameType::kNv12GpuMemoryBuffer,
                                         TestFrameType::kNv12Software,
                                         TestFrameType::kI420};

const VideoTrackRecorder::CodecId kTrackRecorderTestCodec[] = {
    VideoTrackRecorder::CodecId::kVp8, VideoTrackRecorder::CodecId::kVp9
#if BUILDFLAG(RTC_USE_H264)
    ,
    VideoTrackRecorder::CodecId::kH264
#endif
};
const gfx::Size kTrackRecorderTestSize[] = {
    gfx::Size(kVEAEncoderMinResolutionWidth / 2,
              kVEAEncoderMinResolutionHeight / 2),
    gfx::Size(kVEAEncoderMinResolutionWidth, kVEAEncoderMinResolutionHeight)};
static const int kTrackRecorderTestSizeDiff = 20;

constexpr media::VideoCodec MediaVideoCodecFromCodecId(
    VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::kVp8:
      return media::VideoCodec::kVP8;
    case VideoTrackRecorder::CodecId::kVp9:
      return media::VideoCodec::kVP9;
#if BUILDFLAG(RTC_USE_H264)
    case VideoTrackRecorder::CodecId::kH264:
      return media::VideoCodec::kH264;
#endif
    default:
      return media::VideoCodec::kUnknown;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::VideoCodec::kUnknown;
}

}  // namespace

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockTestingPlatform : public IOTaskRunnerTestingPlatformSupport {
 public:
  MockTestingPlatform() = default;
  ~MockTestingPlatform() override = default;

  MOCK_METHOD0(GetGpuFactories, media::GpuVideoAcceleratorFactories*());
};

// TODO(crbug/1177593): refactor the test parameter space to something more
// reasonable. Many tests below ignore parts of the space leading to too much
// being tested.
class VideoTrackRecorderTest
    : public TestWithParam<testing::tuple<VideoTrackRecorder::CodecId,
                                          gfx::Size,
                                          bool,
                                          TestFrameType>> {
 public:
  VideoTrackRecorderTest() : mock_source_(new MockMediaStreamVideoSource()) {
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/,
        base::WrapUnique(mock_source_));
    EXPECT_CALL(*mock_source_, OnRequestRefreshFrame())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*mock_source_, OnCapturingLinkSecured(_))
        .Times(testing::AnyNumber());
    auto platform_track = std::make_unique<MediaStreamVideoTrack>(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true /* enabled */);
    track_ = platform_track.get();
    component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        source_, std::move(platform_track));

    // Paranoia checks.
    EXPECT_EQ(component_->Source()->GetPlatformSource(),
              source_->GetPlatformSource());
    EXPECT_TRUE(scheduler::GetSingleThreadTaskRunnerForTesting()
                    ->BelongsToCurrentThread());

    ON_CALL(*platform_, GetGpuFactories()).WillByDefault(Return(nullptr));
  }

  VideoTrackRecorderTest(const VideoTrackRecorderTest&) = delete;
  VideoTrackRecorderTest& operator=(const VideoTrackRecorderTest&) = delete;

  ~VideoTrackRecorderTest() override {
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
        0u /* bits_per_second */,
        scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  MOCK_METHOD0(OnSourceReadyStateEnded, void());

  MOCK_METHOD5(OnEncodedVideo,
               void(const media::Muxer::VideoParameters& params,
                    std::string encoded_data,
                    std::string encoded_alpha,
                    base::TimeTicks timestamp,
                    bool keyframe));

  void Encode(scoped_refptr<media::VideoFrame> frame,
              base::TimeTicks capture_time) {
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

 protected:
  scoped_refptr<media::VideoFrame> CreateFrameForTest(
      TestFrameType frame_type,
      const gfx::Size& frame_size,
      bool encode_alpha_channel,
      int padding) {
    const gfx::Size padded_size(frame_size.width() + padding,
                                frame_size.height());
    if (frame_type == TestFrameType::kI420) {
      return media::VideoFrame::CreateZeroInitializedFrame(
          encode_alpha_channel ? media::PIXEL_FORMAT_I420A
                               : media::PIXEL_FORMAT_I420,
          padded_size, gfx::Rect(frame_size), frame_size, base::TimeDelta());
    }

    scoped_refptr<media::VideoFrame> video_frame = blink::CreateTestFrame(
        padded_size, gfx::Rect(frame_size), frame_size,
        frame_type == TestFrameType::kNv12Software
            ? media::VideoFrame::STORAGE_OWNED_MEMORY
            : media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
        media::VideoPixelFormat::PIXEL_FORMAT_NV12);
    scoped_refptr<media::VideoFrame> video_frame2 = video_frame;
    if (frame_type == TestFrameType::kNv12GpuMemoryBuffer)
      video_frame2 = media::ConvertToMemoryMappedFrame(video_frame);

    // Fade to black.
    const uint8_t kBlackY = 0x00;
    const uint8_t kBlackUV = 0x80;
    memset(video_frame2->writable_data(0), kBlackY,
           video_frame2->stride(0) * frame_size.height());
    memset(video_frame2->writable_data(1), kBlackUV,
           video_frame2->stride(1) * (frame_size.height() / 2));
    if (frame_type == TestFrameType::kNv12GpuMemoryBuffer)
      return video_frame;
    return video_frame2;
  }
};

// Construct and destruct all objects, in particular |video_track_recorder_| and
// its inner object(s). This is a non trivial sequence.
TEST_P(VideoTrackRecorderTest, ConstructAndDestruct) {
  InitializeRecorder(testing::get<0>(GetParam()));
}

TEST_F(VideoTrackRecorderTest, RelaysReadyStateEnded) {
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);
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
  const TestFrameType test_frame_type = testing::get<3>(GetParam());

  // We don't support alpha channel with GpuMemoryBuffer frames.
  if (test_frame_type != TestFrameType::kI420 && encode_alpha_channel) {
    return;
  }

  const scoped_refptr<media::VideoFrame> video_frame = CreateFrameForTest(
      test_frame_type, frame_size, encode_alpha_channel, /*padding=*/0);
  if (!video_frame)
    ASSERT_TRUE(!!video_frame);

  const double kFrameRate = 60.0f;
  video_frame->metadata().frame_rate = kFrameRate;

  InSequence s;
  const base::TimeTicks timeticks_now = base::TimeTicks::Now();
  base::StringPiece first_frame_encoded_data;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, timeticks_now, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&first_frame_encoded_data),
                      SaveArg<2>(&first_frame_encoded_alpha)));

  // Send another Video Frame.
  const base::TimeTicks timeticks_later = base::TimeTicks::Now();
  base::StringPiece second_frame_encoded_data;
  base::StringPiece second_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, timeticks_later, false))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&second_frame_encoded_data),
                      SaveArg<2>(&second_frame_encoded_alpha)));

  // Send another Video Frame and expect only an OnEncodedVideo() callback.
  const gfx::Size frame_size2(frame_size.width() + kTrackRecorderTestSizeDiff,
                              frame_size.height());
  const scoped_refptr<media::VideoFrame> video_frame2 = CreateFrameForTest(
      test_frame_type, frame_size2, encode_alpha_channel, /*padding=*/0);

  base::RunLoop run_loop;

  base::StringPiece third_frame_encoded_data;
  base::StringPiece third_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&third_frame_encoded_data),
                      SaveArg<2>(&third_frame_encoded_alpha),
                      RunClosure(run_loop.QuitClosure())));
  // A test-only TSAN problem is fixed by placing the encodes down here and not
  // close to the expectation setups.
  Encode(video_frame, timeticks_now);
  Encode(video_frame, timeticks_later);
  Encode(video_frame2, base::TimeTicks::Now());

  run_loop.Run();

  const size_t kEncodedSizeThreshold = 14;
  EXPECT_GE(first_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(second_frame_encoded_data.size(), kEncodedSizeThreshold);
  EXPECT_GE(third_frame_encoded_data.size(), kEncodedSizeThreshold);

  // We only support NV12 with GpuMemoryBuffer video frame.
  if (test_frame_type == TestFrameType::kI420 && encode_alpha_channel &&
      CanEncodeAlphaChannel()) {
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
  const TestFrameType test_frame_type = testing::get<3>(GetParam());
  scoped_refptr<media::VideoFrame> video_frame =
      CreateFrameForTest(test_frame_type, frame_size,
                         /*encode_alpha_channel=*/false, kCodedSizePadding);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Encode(video_frame, base::TimeTicks::Now());
  run_loop.Run();

  Mock::VerifyAndClearExpectations(this);
}

TEST_P(VideoTrackRecorderTest, EncodeFrameRGB) {
  InitializeRecorder(testing::get<0>(GetParam()));

  const gfx::Size& frame_size = testing::get<1>(GetParam());
  // TODO(crbug/1177593): Refactor test harness to use a cleaner parameter
  // space.
  if (testing::get<2>(GetParam()))
    return;
  // TODO(crbug/1177593): Refactor test harness to use a cleaner parameter
  // space.
  // Let kI420 indicate owned memory, and kNv12GpuMemoryBuffer to indicate GMB
  // storage. Don't test for kNv12Software.
  const TestFrameType test_frame_type = testing::get<3>(GetParam());
  if (test_frame_type == TestFrameType::kNv12Software)
    return;

  scoped_refptr<media::VideoFrame> video_frame =
      test_frame_type == TestFrameType::kI420
          ? media::VideoFrame::CreateZeroInitializedFrame(
                media::PIXEL_FORMAT_XRGB, frame_size, gfx::Rect(frame_size),
                frame_size, base::TimeDelta())
          : blink::CreateTestFrame(frame_size, gfx::Rect(frame_size),
                                   frame_size,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_XRGB);

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
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<media::VideoFrame> opaque_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

  InSequence s;
  base::StringPiece first_frame_encoded_alpha;
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true))
      .Times(1)
      .WillOnce(SaveArg<2>(&first_frame_encoded_alpha));
  Encode(opaque_frame, base::TimeTicks::Now());

  const scoped_refptr<media::VideoFrame> alpha_frame =
      media::VideoFrame::CreateTransparentFrame(frame_size);
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
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

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
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

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
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  const gfx::Size& frame_size = kTrackRecorderTestSize[0];
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size);

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
  InitializeRecorder(VideoTrackRecorder::CodecId::kVp8);

  EXPECT_EQ(video_track_recorder_->GetRequiredMinFramesPerSec(), 1);

  test::RunDelayedTasks(base::Seconds(1));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoTrackRecorderTest,
                         ::testing::Combine(ValuesIn(kTrackRecorderTestCodec),
                                            ValuesIn(kTrackRecorderTestSize),
                                            ::testing::Bool(),
                                            ValuesIn(kTestFrameTypes)));

class VideoTrackRecorderPassthroughTest
    : public TestWithParam<VideoTrackRecorder::CodecId> {
 public:
  using CodecId = VideoTrackRecorder::CodecId;

  VideoTrackRecorderPassthroughTest()
      : mock_source_(new MockMediaStreamVideoSource()) {
    ON_CALL(*mock_source_, SupportsEncodedOutput).WillByDefault(Return(true));
    const String track_id("dummy");
    source_ = MakeGarbageCollected<MediaStreamSource>(
        track_id, MediaStreamSource::kTypeVideo, track_id, false /*remote*/,
        base::WrapUnique(mock_source_));
    component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        source_, std::make_unique<MediaStreamVideoTrack>(
                     mock_source_,
                     WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
                     true /* enabled */));

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
               void(const media::Muxer::VideoParameters& params,
                    std::string encoded_data,
                    std::string encoded_alpha,
                    base::TimeTicks timestamp,
                    bool is_key_frame));

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |source_|.
  MockMediaStreamVideoSource* mock_source_;
  Persistent<MediaStreamSource> source_;
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
  auto frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
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
  frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
  EXPECT_CALL(*this, OnEncodedVideo(_, _, _, _, true));
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);

  // Frame 3 (deltaframe) - forwarded
  base::RunLoop run_loop;
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
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
  auto frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  video_track_recorder_->Pause();

  // Expect no frame throughput now.
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
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
  frame = CreateFrame(/*is_key_frame=*/false, CodecId::kVp9);
  EXPECT_CALL(*this, OnEncodedVideo).Times(0);
  video_track_recorder_->OnEncodedVideoFrameForTesting(frame,
                                                       base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(this);

  frame = CreateFrame(/*is_key_frame=*/true, CodecId::kVp9);
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

  CodecEnumeratorTest(const CodecEnumeratorTest&) = delete;
  CodecEnumeratorTest& operator=(const CodecEnumeratorTest&) = delete;

  ~CodecEnumeratorTest() override = default;

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp8Profiles() {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    profiles.emplace_back(media::VP8PROFILE_ANY, gfx::Size(1920, 1080), 30, 1);
    return profiles;
  }

  media::VideoEncodeAccelerator::SupportedProfiles MakeVp9Profiles(
      bool vbr_support = false) {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    auto rc_mode =
        media::VideoEncodeAccelerator::SupportedRateControlMode::kConstantMode;
    if (vbr_support) {
      rc_mode |= media::VideoEncodeAccelerator::SupportedRateControlMode::
          kVariableMode;
    }

    profiles.emplace_back(media::VP9PROFILE_PROFILE1, gfx::Size(1920, 1080), 60,
                          1, rc_mode);
    profiles.emplace_back(media::VP9PROFILE_PROFILE2, gfx::Size(1920, 1080), 30,
                          1, rc_mode);
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

  media::VideoEncodeAccelerator::SupportedProfiles MakeH264Profiles(
      bool vbr_support = false) {
    media::VideoEncodeAccelerator::SupportedProfiles profiles;
    auto rc_mode =
        media::VideoEncodeAccelerator::SupportedRateControlMode::kConstantMode;
    if (vbr_support) {
      rc_mode |= media::VideoEncodeAccelerator::SupportedRateControlMode::
          kVariableMode;
    }

    profiles.emplace_back(media::H264PROFILE_BASELINE, gfx::Size(1920, 1080),
                          24, 1, rc_mode);
    profiles.emplace_back(media::H264PROFILE_MAIN, gfx::Size(1920, 1080), 30, 1,
                          rc_mode);
    profiles.emplace_back(media::H264PROFILE_HIGH, gfx::Size(1920, 1080), 60, 1,
                          rc_mode);
    return profiles;
  }
};

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdDefault) {
  // Empty supported profiles.
  const CodecEnumerator emulator(
      (media::VideoEncodeAccelerator::SupportedProfiles()));
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8) {
  const CodecEnumerator emulator(MakeVp8Profiles());
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(CodecId::kVp9, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, GetPreferredCodecIdVp8Vp9) {
  const CodecEnumerator emulator(MakeVp8Vp9Profiles());
  EXPECT_EQ(CodecId::kVp8, emulator.GetPreferredCodecId());
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::kVp9);
  EXPECT_EQ(2u, profiles.size());
  EXPECT_EQ(media::VP9PROFILE_PROFILE1, profiles[0].profile);
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, profiles[1].profile);
}

TEST_F(CodecEnumeratorTest, MakeSupportedProfilesNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      emulator.GetSupportedProfiles(CodecId::kVp8);
  EXPECT_TRUE(profiles.empty());
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileVp9) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(std::make_pair(media::VP9PROFILE_PROFILE1, /*vbr_support=*/false),
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp9));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileNoVp8) {
  const CodecEnumerator emulator(MakeVp9Profiles());
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp8));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileVp9VBR) {
  const CodecEnumerator emulator(MakeVp9Profiles(/*vbr_support=*/true));
  EXPECT_EQ(std::make_pair(media::VP9PROFILE_PROFILE1, /*vbr_support=*/true),
            emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp9));
}

TEST_F(CodecEnumeratorTest, GetFirstSupportedVideoCodecProfileNoVp8VBR) {
  const CodecEnumerator emulator(MakeVp9Profiles(/*vbr_support=*/true));
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.GetFirstSupportedVideoCodecProfile(CodecId::kVp8));
}

#if BUILDFLAG(RTC_USE_H264)
TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(std::make_pair(media::H264PROFILE_HIGH, /*vbr_support=*/false),
            emulator.FindSupportedVideoCodecProfile(CodecId::kH264,
                                                    media::H264PROFILE_HIGH));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileH264VBR) {
  const CodecEnumerator emulator(MakeH264Profiles(/*vbr_support=*/true));
  EXPECT_EQ(std::make_pair(media::H264PROFILE_HIGH, /*vbr_support=*/true),
            emulator.FindSupportedVideoCodecProfile(CodecId::kH264,
                                                    media::H264PROFILE_HIGH));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileNoProfileH264) {
  const CodecEnumerator emulator(MakeH264Profiles());
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.FindSupportedVideoCodecProfile(
          CodecId::kH264, media::H264PROFILE_HIGH422PROFILE));
}

TEST_F(CodecEnumeratorTest, FindSupportedVideoCodecProfileNoProfileH264VBR) {
  const CodecEnumerator emulator(MakeH264Profiles(/*vbr_support=*/true));
  EXPECT_EQ(
      std::make_pair(media::VIDEO_CODEC_PROFILE_UNKNOWN, /*vbr_support=*/false),
      emulator.FindSupportedVideoCodecProfile(
          CodecId::kH264, media::H264PROFILE_HIGH422PROFILE));
}

#endif

}  // namespace blink
