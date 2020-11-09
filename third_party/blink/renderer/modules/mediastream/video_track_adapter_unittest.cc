// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"

#include <limits>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"

namespace blink {

// Most VideoTrackAdapter functionality is tested in MediaStreamVideoSourceTest.
// These tests focus on the computation of cropped frame sizes in edge cases
// that cannot be easily reproduced by a mocked video source, such as tests
// involving frames of zero size.
// Such frames can be produced by sources in the wild (e.g., element capture).

// Test that cropped sizes with zero-area input frames are correctly computed.
// Aspect ratio limits should be ignored.
TEST(VideoTrackAdapterTest, ZeroInputArea) {
  const int kMaxWidth = 640;
  const int kMaxHeight = 480;
  const int kSmallDimension = 300;
  const int kLargeDimension = 1000;
  static_assert(kSmallDimension < kMaxWidth && kSmallDimension < kMaxHeight,
                "kSmallDimension must be less than kMaxWidth and kMaxHeight");
  static_assert(
      kLargeDimension > kMaxWidth && kLargeDimension > kMaxHeight,
      "kLargeDimension must be greater than kMaxWidth and kMaxHeight");
  const VideoTrackAdapterSettings kVideoTrackAdapterSettings(
      gfx::Size(kMaxWidth, kMaxHeight), 0.1 /* min_aspect_ratio */,
      2.0 /* max_aspect_ratio */, 0.0 /* max_frame_rate */);
  const bool kIsRotatedValues[] = {true, false};

  for (bool is_rotated : kIsRotatedValues) {
    gfx::Size desired_size;

    VideoTrackAdapter::CalculateDesiredSize(
        is_rotated, gfx::Size(0, 0), kVideoTrackAdapterSettings, &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), 0);

    // Zero width.
    VideoTrackAdapter::CalculateDesiredSize(
        is_rotated, gfx::Size(0, kSmallDimension), kVideoTrackAdapterSettings,
        &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), kSmallDimension);

    // Zero height.
    VideoTrackAdapter::CalculateDesiredSize(
        is_rotated, gfx::Size(kSmallDimension, 0), kVideoTrackAdapterSettings,
        &desired_size);
    EXPECT_EQ(desired_size.width(), kSmallDimension);
    EXPECT_EQ(desired_size.height(), 0);

    // Requires "cropping" of height.
    VideoTrackAdapter::CalculateDesiredSize(
        is_rotated, gfx::Size(0, kLargeDimension), kVideoTrackAdapterSettings,
        &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), is_rotated ? kMaxWidth : kMaxHeight);

    // Requires "cropping" of width.
    VideoTrackAdapter::CalculateDesiredSize(
        is_rotated, gfx::Size(kLargeDimension, 0), kVideoTrackAdapterSettings,
        &desired_size);
    EXPECT_EQ(desired_size.width(), is_rotated ? kMaxHeight : kMaxWidth);
    EXPECT_EQ(desired_size.height(), 0);
  }
}

// Test that zero-size cropped areas are correctly computed. Aspect ratio
// limits should be ignored.
TEST(VideoTrackAdapterTest, ZeroOutputArea) {
  const double kMinAspectRatio = 0.1;
  const double kMaxAspectRatio = 2.0;
  const int kInputWidth = 640;
  const int kInputHeight = 480;
  const int kSmallMaxDimension = 300;
  const int kLargeMaxDimension = 1000;
  static_assert(
      kSmallMaxDimension < kInputWidth && kSmallMaxDimension < kInputHeight,
      "kSmallMaxDimension must be less than kInputWidth and kInputHeight");
  static_assert(
      kLargeMaxDimension > kInputWidth && kLargeMaxDimension > kInputHeight,
      "kLargeMaxDimension must be greater than kInputWidth and kInputHeight");

  gfx::Size desired_size;

  VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(gfx::Size(0, 0), kMinAspectRatio,
                                kMaxAspectRatio, 0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), 0);

  // Width is cropped to zero.
  VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(gfx::Size(0, kLargeMaxDimension), 0.0,
                                HUGE_VAL,  // kMinAspectRatio, kMaxAspectRatio,
                                0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), kInputHeight);

  // Requires "cropping" of width and height.
  VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(gfx::Size(0, kSmallMaxDimension),
                                kMinAspectRatio, kMaxAspectRatio,
                                0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), kSmallMaxDimension);

  // Height is cropped to zero.
  VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(gfx::Size(kLargeMaxDimension, 0),
                                kMinAspectRatio, kMaxAspectRatio,
                                0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_EQ(desired_size.width(), kInputWidth);
  EXPECT_EQ(desired_size.height(), 0);

  // Requires "cropping" of width and height.
  VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(
          gfx::Size(kSmallMaxDimension /* max_width */, 0 /* max_height */),
          kMinAspectRatio, kMaxAspectRatio, 0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_EQ(desired_size.width(), kSmallMaxDimension);
  EXPECT_EQ(desired_size.height(), 0);
}

// Test that large frames are handled correctly.
TEST(VideoTrackAdapterTest, LargeFrames) {
  const int kInputWidth = std::numeric_limits<int>::max();
  const int kInputHeight = std::numeric_limits<int>::max();
  const int kMaxWidth = std::numeric_limits<int>::max();
  const int kMaxHeight = std::numeric_limits<int>::max();

  gfx::Size desired_size;

  // If a target size is provided in VideoTrackAdapterSettings, rescaling is
  // allowed and frames must be clamped to the maximum allowed size, even if the
  // target exceeds the system maximum.
  bool success = VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(gfx::Size(kMaxWidth, kMaxHeight),
                                0.0 /* max_frame_rate */),
      &desired_size);
  EXPECT_TRUE(success);
  EXPECT_EQ(desired_size.width(), media::limits::kMaxDimension);
  EXPECT_EQ(desired_size.height(), media::limits::kMaxDimension);

  // If no target size is provided in VideoTrackAdapterSettings, rescaling is
  // disabled and |desired_size| is left unmodified.
  desired_size.set_width(0);
  desired_size.set_height(0);
  success = VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(), &desired_size);
  EXPECT_FALSE(success);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), 0);
}

// Test that regular frames are not rescaled if settings do not specify a target
// resolution.
TEST(VideoTrackAdapterTest, NoRescaling) {
  const int kInputWidth = 640;
  const int kInputHeight = 480;

  // No target size,
  gfx::Size desired_size;
  bool success = VideoTrackAdapter::CalculateDesiredSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      VideoTrackAdapterSettings(), &desired_size);
  EXPECT_TRUE(success);
  EXPECT_EQ(desired_size.width(), kInputWidth);
  EXPECT_EQ(desired_size.height(), kInputHeight);
}

class VideoTrackAdapterFixtureTest : public ::testing::Test {
 public:
  VideoTrackAdapterFixtureTest()
      : testing_render_thread_("TestingRenderThread"),
        frame_received_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~VideoTrackAdapterFixtureTest() override = default;

 protected:
  void SetUp() override { testing_render_thread_.Start(); }

  void TearDown() override {
    if (track_added_) {
      testing_render_thread_.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&VideoTrackAdapter::RemoveTrack, adapter_,
                                    null_track_.get()));
    }
    testing_render_thread_.Stop();
  }

  void CreateAdapter(media::VideoCaptureFormat capture_format) {
    mock_source_ =
        std::make_unique<MockMediaStreamVideoSource>(capture_format, false);
    // Create the VideoTrackAdapter instance on |testing_render_thread_|.
    base::WaitableEvent adapter_created(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          adapter_ = base::MakeRefCounted<VideoTrackAdapter>(
              platform_support_->GetIOTaskRunner(), mock_source_->GetWeakPtr());
          adapter_created.Signal();
        }));
    adapter_created.Wait();
  }

  // Create or re-configure the dummy |null_track_| with the given
  // |adapter_settings|.
  void ConfigureTrack(const VideoTrackAdapterSettings& adapter_settings) {
    if (!track_added_) {
      AddTrackInternal(null_track_.get(), adapter_settings);
      track_added_ = true;
    } else {
      testing_render_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoTrackAdapter::ReconfigureTrack, adapter_,
                         null_track_.get(), adapter_settings));
    }
  }

  void AddTrackInternal(MediaStreamVideoTrack* track,
                        const VideoTrackAdapterSettings& adapter_settings) {
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoTrackAdapter::AddTrack, adapter_, track,
            base::BindRepeating(&VideoTrackAdapterFixtureTest::OnFrameDelivered,
                                base::Unretained(this)),
            base::BindRepeating(
                &VideoTrackAdapterFixtureTest::OnEncodedVideoFrameDelivered,
                base::Unretained(this)),
            base::DoNothing(), base::DoNothing(), adapter_settings));
  }

  void SetFrameValidationCallback(VideoCaptureDeliverFrameCB callback) {
    frame_validation_callback_ = std::move(callback);
  }

  // Deliver |frame| to |adapter_| and wait until OnFrameDelivered signals that
  // it receives the processed frame.
  void DeliverAndValidateFrame(scoped_refptr<media::VideoFrame> frame,
                               base::TimeTicks estimated_capture_time) {
    auto deliver_frame = [&]() {
      platform_support_->GetIOTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&VideoTrackAdapter::DeliverFrameOnIO,
                                    adapter_, frame, estimated_capture_time));
    };

    frame_received_.Reset();
    // Bounce the call to DeliverFrameOnIO off |testing_render_thread_| to
    // synchronize with the AddTrackOnIO / ReconfigureTrackOnIO that would be
    // invoked through ConfigureTrack.
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(deliver_frame));
    frame_received_.Wait();
  }

  void OnFrameDelivered(scoped_refptr<media::VideoFrame> frame,
                        base::TimeTicks estimated_capture_time) {
    if (frame_validation_callback_) {
      frame_validation_callback_.Run(frame, estimated_capture_time);
    }
    frame_received_.Signal();
  }

  MOCK_METHOD2(OnEncodedVideoFrameDelivered,
               void(scoped_refptr<EncodedVideoFrame>,
                    base::TimeTicks estimated_capture_time));

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport>
      platform_support_;
  base::Thread testing_render_thread_;
  std::unique_ptr<MockMediaStreamVideoSource> mock_source_;
  scoped_refptr<VideoTrackAdapter> adapter_;

  base::WaitableEvent frame_received_;
  VideoCaptureDeliverFrameCB frame_validation_callback_;

  // For testing we use a nullptr for MediaStreamVideoTrack.
  std::unique_ptr<MediaStreamVideoTrack> null_track_ = nullptr;
  bool track_added_ = false;
};

TEST_F(VideoTrackAdapterFixtureTest, DeliverFrame_GpuMemoryBuffer) {
  // Attributes for the original input frame.
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(1280, 720);
  const double kFrameRate = 30.0;
  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // Initialize the VideoTrackAdapter to handle GpuMemoryBuffer. NV12 is the
  // only pixel format supported at the moment.
  const media::VideoCaptureFormat stream_format(kCodedSize, kFrameRate,
                                                media::PIXEL_FORMAT_NV12);
  CreateAdapter(stream_format);

  // Keep the desired size the same as the natural size of the original frame.
  VideoTrackAdapterSettings settings_nonscaled(kNaturalSize, kFrameRate);
  ConfigureTrack(settings_nonscaled);
  auto check_nonscaled = [&](scoped_refptr<media::VideoFrame> frame,
                             base::TimeTicks estimated_capture_time) {
    // We should get the original frame as-is here.
    EXPECT_EQ(frame->storage_type(),
              media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
    EXPECT_EQ(frame->GetGpuMemoryBuffer(), gmb_frame->GetGpuMemoryBuffer());
    EXPECT_EQ(frame->coded_size(), kCodedSize);
    EXPECT_EQ(frame->visible_rect(), kVisibleRect);
    EXPECT_EQ(frame->natural_size(), kNaturalSize);
  };
  SetFrameValidationCallback(base::BindLambdaForTesting(check_nonscaled));
  DeliverAndValidateFrame(gmb_frame, base::TimeTicks());

  // Scale the original frame by a factor of 0.5x.
  const gfx::Size kDesiredSize(640, 360);
  VideoTrackAdapterSettings settings_scaled(kDesiredSize, kFrameRate);
  ConfigureTrack(settings_scaled);
  auto check_scaled = [&](scoped_refptr<media::VideoFrame> frame,
                          base::TimeTicks estimated_capture_time) {
    // The original frame should be wrapped in a new frame, with |kDesiredSize|
    // exposed as natural size of the wrapped frame.
    EXPECT_EQ(frame->storage_type(),
              media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
    EXPECT_EQ(frame->GetGpuMemoryBuffer(), gmb_frame->GetGpuMemoryBuffer());
    EXPECT_EQ(frame->coded_size(), kCodedSize);
    EXPECT_EQ(frame->visible_rect(), kVisibleRect);
    EXPECT_EQ(frame->natural_size(), kDesiredSize);
  };
  SetFrameValidationCallback(base::BindLambdaForTesting(check_scaled));
  DeliverAndValidateFrame(gmb_frame, base::TimeTicks());
}

class VideoTrackAdapterEncodedTest : public ::testing::Test {
 public:
  VideoTrackAdapterEncodedTest()
      : render_thread_("VideoTrackAdapterEncodedTest_RenderThread") {
    render_thread_.Start();
    auto source = std::make_unique<MockMediaStreamVideoSource>(
        media::VideoCaptureFormat(), false);
    mock_source_ = source.get();
    web_source_.Initialize(
        blink::WebString::FromASCII("source_id"),
        blink::WebMediaStreamSource::kTypeVideo,
        blink::WebString::FromASCII("DeliverEncodedVideoFrameSource"),
        false /* remote */);
    web_source_.SetPlatformSource(std::move(source));
    RunSyncOnRenderThread([&] {
      adapter_ = base::MakeRefCounted<VideoTrackAdapter>(
          platform_support_->GetIOTaskRunner(), mock_source_->GetWeakPtr());
    });
  }

  ~VideoTrackAdapterEncodedTest() {
    web_source_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  std::unique_ptr<MediaStreamVideoTrack> AddTrack() {
    auto track = std::make_unique<MediaStreamVideoTrack>(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true);
    RunSyncOnRenderThread([&] {
      adapter_->AddTrack(
          track.get(),
          base::BindRepeating(&VideoTrackAdapterEncodedTest::OnFrameDelivered,
                              base::Unretained(this)),
          base::BindRepeating(
              &VideoTrackAdapterEncodedTest::OnEncodedVideoFrameDelivered,
              base::Unretained(this)),
          base::DoNothing(), base::DoNothing(), VideoTrackAdapterSettings());
    });
    return track;
  }

  template <class Function>
  void RunSyncOnRenderThread(Function function) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    render_thread_.task_runner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&] {
                                             std::move(function)();
                                             std::move(quit_closure).Run();
                                           }));
    run_loop.Run();
  }

  MOCK_METHOD2(OnFrameDelivered,
               void(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks estimated_capture_time));
  MOCK_METHOD2(OnEncodedVideoFrameDelivered,
               void(scoped_refptr<EncodedVideoFrame>,
                    base::TimeTicks estimated_capture_time));

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport>
      platform_support_;
  base::Thread render_thread_;
  WebMediaStreamSource web_source_;
  MockMediaStreamVideoSource* mock_source_;
  scoped_refptr<VideoTrackAdapter> adapter_;
};

TEST_F(VideoTrackAdapterEncodedTest, DeliverEncodedVideoFrame) {
  auto track1 = AddTrack();
  auto track2 = AddTrack();
  EXPECT_CALL(*this, OnEncodedVideoFrameDelivered)
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([&](const scoped_refptr<EncodedVideoFrame>& frame,
                              base::TimeTicks estimated_capture_time) {}));
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  platform_support_->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        adapter_->DeliverEncodedVideoFrameOnIO(
            base::MakeRefCounted<MockEncodedVideoFrame>(), base::TimeTicks());
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
  RunSyncOnRenderThread([&] {
    adapter_->RemoveTrack(track1.get());
    adapter_->RemoveTrack(track2.get());
  });
}

}  // namespace blink
