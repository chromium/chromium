// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"

#include <limits>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/ranges.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/webrtc_overrides/low_precision_timer.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

// Most VideoTrackAdapter functionality is tested in MediaStreamVideoSourceTest.
// These tests focus on the computation of cropped frame sizes in edge cases
// that cannot be easily reproduced by a mocked video source, such as tests
// involving frames of zero size.
// Such frames can be produced by sources in the wild (e.g., element capture).

// Test that cropped sizes with zero-area input frames are correctly computed.
// Aspect ratio limits should be ignored.
TEST(VideoTrackAdapterTest, ZeroInputArea) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
        frame_processed_(base::WaitableEvent::ResetPolicy::MANUAL,
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
    base::WaitableEvent source_deleted;
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mock_source_.reset();
          source_deleted.Signal();
        }));
    source_deleted.Wait();
    testing_render_thread_.Stop();
  }

  void CreateAdapter(media::VideoCaptureFormat capture_format) {
    // Create the MockMediaStreamVideoSource and VideoTrackAdapter instances on
    // |testing_render_thread_|.
    base::WaitableEvent adapter_created(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mock_source_ = std::make_unique<NiceMock<MockMediaStreamVideoSource>>(
              capture_format, false);
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
            base::BindRepeating(&VideoTrackAdapterFixtureTest::OnFrameDropped,
                                base::Unretained(this)),
            base::BindRepeating(
                &VideoTrackAdapterFixtureTest::OnEncodedVideoFrameDelivered,
                base::Unretained(this)),
            /*sub_capture_target_version_callback=*/base::DoNothing(),
            /*settings_callback=*/base::DoNothing(),
            /*format_callback=*/base::DoNothing(), adapter_settings));
  }

  void SetFrameValidationCallback(VideoCaptureDeliverFrameCB callback) {
    frame_validation_callback_ = std::move(callback);
  }

  void SetFrameDroppedCallback(VideoCaptureNotifyFrameDroppedCB callback) {
    frame_dropped_callback_ = std::move(callback);
  }

  // Deliver |frame| to |adapter_| and wait until OnFrameDelivered signals that
  // it receives the processed frame.
  void DeliverAndValidateFrame(scoped_refptr<media::VideoFrame> frame,
                               base::TimeTicks estimated_capture_time) {
    auto deliver_frame = [&]() {
      platform_support_->GetIOTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoTrackAdapter::DeliverFrameOnVideoTaskRunner,
                         adapter_, frame,
                         estimated_capture_time));
    };

    frame_processed_.Reset();
    // Bounce the call to DeliverFrameOnVideoTaskRunner off
    // |testing_render_thread_| to synchronize with the
    // AddTrackOnVideoTaskRunner / ReconfigureTrackOnVideoTaskRunner that would
    // be invoked through ConfigureTrack.
    testing_render_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(deliver_frame));
    frame_processed_.Wait();
  }

  void OnFrameDelivered(
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks estimated_capture_time) {
    if (frame_validation_callback_) {
      frame_validation_callback_.Run(frame, estimated_capture_time);
      frame_processed_.Signal();
    }
  }

  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) {
    if (frame_dropped_callback_) {
      frame_dropped_callback_.Run(reason);
      frame_processed_.Signal();
    }
  }

  // Configures a track and an adapter with the given target frame rate,
  // generates `num_frames` frames with 10x10 resolution. The timestamps are
  // generated by calling the lambda function `index_to_timestamp` for each
  // frame. Returns the number of delivered and dropped frames.
  std::tuple<int, int> GenerateAndCountFrames(
      int num_frames,
      std::optional<double> target_frame_rate,
      base::RepeatingCallback<base::TimeDelta(int)> index_to_timestamp) {
    const gfx::Size resolution(10, 10);
    // Any capture format will work. Frames will generated at the
    // `actual_input_frame_rate`.
    const media::VideoCaptureFormat stream_format(
        resolution, /*frame_rate=*/10.0, media::PIXEL_FORMAT_I420);
    CreateAdapter(stream_format);

    VideoTrackAdapterSettings adapter_settings(
        /*target_size=*/std::nullopt,
        /*min_aspect_ratio=*/0.0000,
        /*max_aspect_ratio=*/resolution.width(), target_frame_rate);
    ConfigureTrack(adapter_settings);
    base::WaitableEvent did_process_all_frames;
    int num_delivered = 0;
    int num_dropped = 0;

    SetFrameValidationCallback(base::BindLambdaForTesting(
        [&](scoped_refptr<media::VideoFrame> frame,
            base::TimeTicks estimated_capture_time) {
          num_delivered++;
          if (num_delivered + num_dropped == num_frames) {
            did_process_all_frames.Signal();
          }
        }));
    SetFrameDroppedCallback(
        base::BindLambdaForTesting([&](media::VideoCaptureFrameDropReason) {
          num_dropped++;
          if (num_delivered + num_dropped == num_frames) {
            did_process_all_frames.Signal();
          }
        }));

    for (int i = 0; i < num_frames; ++i) {
      auto frame = CreateTestFrame(
          /*coded_size=*/resolution,
          /*visible_rect=*/
          gfx::Rect(0, 0, resolution.width(), resolution.height()),
          /*natural_size*/ resolution,
          /*storage_type=*/media::VideoFrame::STORAGE_OWNED_MEMORY,
          media::PIXEL_FORMAT_I420,
          /*timestamp=*/index_to_timestamp.Run(i));
      DeliverAndValidateFrame(std::move(frame), base::TimeTicks());
    }

    did_process_all_frames.Wait();
    return {num_delivered, num_dropped};
  }

  // Configures a track and an adapter with the given target frame rate,
  // generates `num_frames` frames with 10x10 resolution at the given
  // `actual_input_frame_rate` and returns the number of delivered and dropped
  // frames.
  std::tuple<int, int> GenerateAndCountFrames(
      int num_frames,
      std::optional<double> target_frame_rate,
      double actual_input_frame_rate) {
    auto index_to_timestamp =
        base::BindLambdaForTesting([actual_input_frame_rate](int i) {
          return i * base::Seconds(1.0 / actual_input_frame_rate);
        });
    return GenerateAndCountFrames(num_frames, target_frame_rate,
                                  index_to_timestamp);
  }

  void TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      scoped_refptr<media::VideoFrame> frame,
      media::VideoPixelFormat pixel_format,
      gfx::Size desired_size) {
    const gfx::Size coded_size = frame->coded_size();
    const double kFrameRate = 30.0;
    const media::VideoCaptureFormat stream_format(coded_size, kFrameRate,
                                                  pixel_format);
    CreateAdapter(stream_format);

    VideoTrackAdapterSettings cropped_settings(desired_size, kFrameRate);
    ConfigureTrack(cropped_settings);
    auto check_settings =
        [&](scoped_refptr<media::VideoFrame> frame,
            base::TimeTicks estimated_capture_time) {
          EXPECT_FALSE(frame->visible_rect().x() & 1);
          EXPECT_FALSE(frame->visible_rect().y() & 1);
        };
    SetFrameValidationCallback(base::BindLambdaForTesting(check_settings));
    DeliverAndValidateFrame(frame, base::TimeTicks());
  }

  MOCK_METHOD2(OnEncodedVideoFrameDelivered,
               void(scoped_refptr<EncodedVideoFrame>,
                    base::TimeTicks estimated_capture_time));

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport>
      platform_support_;
  test::TaskEnvironment task_environment_;
  base::Thread testing_render_thread_;
  std::unique_ptr<NiceMock<MockMediaStreamVideoSource>> mock_source_;
  scoped_refptr<VideoTrackAdapter> adapter_;

  base::WaitableEvent frame_processed_;
  VideoCaptureDeliverFrameCB frame_validation_callback_;
  VideoCaptureNotifyFrameDroppedCB frame_dropped_callback_;

  // For testing we use a nullptr for MediaStreamVideoTrack.
  std::unique_ptr<MediaStreamVideoTrack> null_track_;
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
  auto check_nonscaled =
      [&](scoped_refptr<media::VideoFrame> frame,
          base::TimeTicks estimated_capture_time) {
        // We should get the original frame as-is here.
        EXPECT_EQ(frame->storage_type(),
                  media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
        EXPECT_EQ(frame->GetGpuMemoryBufferForTesting(),
                  gmb_frame->GetGpuMemoryBufferForTesting());
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
  auto check_scaled =
      [&](scoped_refptr<media::VideoFrame> frame,
          base::TimeTicks estimated_capture_time) {
        // The original frame should be wrapped in a new frame, with
        // |kDesiredSize| exposed as natural size of the wrapped frame.
        EXPECT_EQ(frame->storage_type(),
                  media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
        EXPECT_EQ(frame->GetGpuMemoryBufferForTesting(),
                  gmb_frame->GetGpuMemoryBufferForTesting());
        EXPECT_EQ(frame->coded_size(), kCodedSize);
        EXPECT_EQ(frame->visible_rect(), kVisibleRect);
        EXPECT_EQ(frame->natural_size(), kDesiredSize);
      };
  SetFrameValidationCallback(base::BindLambdaForTesting(check_scaled));
  DeliverAndValidateFrame(gmb_frame, base::TimeTicks());
}

TEST_F(VideoTrackAdapterFixtureTest,
       DeliversHWFrameVisibleRectWithEvenOriginAndWidth) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1280, 720);
  const gfx::Size kDesiredSize(1277, 720);
  const gfx::Size kNaturalSize(1280, 720);
  TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER),
      media::PIXEL_FORMAT_NV12, kDesiredSize);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DeliversSWFrameVisibleRectWithEvenOriginAndWidth) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1280, 720);
  const gfx::Size kDesiredSize(1277, 720);
  const gfx::Size kNaturalSize(1280, 720);
  TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY),
      media::PIXEL_FORMAT_I420, kDesiredSize);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DeliversHWFrameVisibleRectWithEvenOriginAndHeight) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1280, 720);
  const gfx::Size kDesiredSize(1280, 718);
  const gfx::Size kNaturalSize(1280, 720);
  TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER),
      media::PIXEL_FORMAT_NV12, kDesiredSize);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DeliversSWFrameVisibleRectWithEvenOriginAndHeight) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1280, 720);
  const gfx::Size kDesiredSize(1280, 718);
  const gfx::Size kNaturalSize(1280, 720);
  TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY),
      media::PIXEL_FORMAT_I420, kDesiredSize);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DeliversSWFrameOddVisibleRectWithEvenOriginAndHeight) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1279, 719);
  const gfx::Size kDesiredSize(1024, 600);
  const gfx::Size kNaturalSize(1280, 720);
  TestDeliversFrameWithVisibleRectWithEvenOriginAndSize(
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY),
      media::PIXEL_FORMAT_I420, kDesiredSize);
}

// Tests that we run the |settings_callback| for any additional tracks that
// share a VideoFrameResolutionAdapter with an existing track. This ensures that
// the additional track's default frame_size and frame_rate are updated to match
// incoming frames.
TEST_F(VideoTrackAdapterFixtureTest,
       DeliverPortraitFrame_RunSettingsCallbackForSecondTrack) {
  // Attributes for initial track's incoming frame.
  const gfx::Size kCodedSize(480, 640);
  const gfx::Rect kVisibleRect(0, 0, 480, 640);
  const gfx::Size kNaturalSize(480, 640);
  const double kFrameRate = 30.0;
  auto test_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      /*storage_type=*/media::VideoFrame::STORAGE_OWNED_MEMORY);

  const media::VideoCaptureFormat stream_format(kCodedSize, kFrameRate,
                                                media::PIXEL_FORMAT_I420);
  CreateAdapter(stream_format);

  // We don't provide a target size for the initial track.
  VideoTrackAdapterSettings adapter_settings(
      /*target_size=*/std::nullopt,
      /*min_aspect_ratio=*/0.0000,
      /*max_aspect_ratio=*/640.0000, kFrameRate);
  ConfigureTrack(adapter_settings);
  // The delivered frame for the first track should have a portrait orientation.
  auto check_portrait =
      [&](scoped_refptr<media::VideoFrame> frame,
          base::TimeTicks estimated_capture_time) {
        // We should get the original frame as-is here.
        EXPECT_EQ(frame->storage_type(),
                  media::VideoFrame::STORAGE_OWNED_MEMORY);
        EXPECT_EQ(frame->coded_size(), kCodedSize);
        EXPECT_EQ(frame->visible_rect(), kVisibleRect);
        EXPECT_EQ(frame->natural_size(), kNaturalSize);
      };
  SetFrameValidationCallback(base::BindLambdaForTesting(check_portrait));
  DeliverAndValidateFrame(test_frame, base::TimeTicks());

  base::WaitableEvent settings_callback_run_;
  // This lambda callback method validates that the |settings_callback|
  // (MediaStreamVideoTrack::SetSizeAndComputedFrameRate) is run with the
  // frame_size & frame_rate stored in the adapter's |track_settings_|. These
  // values should have been set when we delivered a frame for the first
  // track.
  auto check_dimensions = [&](gfx::Size frame_size, double frame_rate) {
    EXPECT_EQ(frame_size, kNaturalSize);
    EXPECT_EQ(frame_rate, kFrameRate);
    settings_callback_run_.Signal();
  };

  // Add an additional track with the same |adapter_settings| as the first
  // track. Because of this it should share a VideoFrameResolutionAdapter with
  // the first track. The lambda callback method should run as part of the
  // VideoTrackAdapter::AddTrack logic.
  std::unique_ptr<MediaStreamVideoTrack> second_track;
  testing_render_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoTrackAdapter::AddTrack, adapter_, second_track.get(),
          /*frame_callback=*/base::DoNothing(),
          /*notify_dropped_frame_callback=*/base::DoNothing(),
          /*encoded_frame_callback=*/base::DoNothing(),
          /*sub_capture_target_version_callback=*/base::DoNothing(),
          /*settings_callback=*/base::BindLambdaForTesting(check_dimensions),
          /*track_callback=*/base::DoNothing(), adapter_settings));
  settings_callback_run_.Wait();
}

TEST_F(VideoTrackAdapterFixtureTest, FrameRateReduction) {
  const double kInputFrameRate = 30.0;
  const double kTargetFrameRate = 10.0;
  const int kNumFrames = 1000;

  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, kTargetFrameRate, kInputFrameRate);
  EXPECT_EQ(num_delivered + num_dropped, kNumFrames);
  EXPECT_TRUE(base::IsApproximatelyEqual(
      static_cast<double>(num_delivered) / kNumFrames,
      kTargetFrameRate / kInputFrameRate, /*tolerance=*/0.05));
}

TEST_F(VideoTrackAdapterFixtureTest, ArbitraryFrameRateReduction) {
  const double kInputFrameRate = 22.0;
  const double kTargetFrameRate = 10.0;
  const int kNumFrames = 1000;

  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, kTargetFrameRate, kInputFrameRate);
  EXPECT_EQ(num_delivered + num_dropped, kNumFrames);
  EXPECT_NEAR(static_cast<double>(num_delivered) / kNumFrames * kInputFrameRate,
              kTargetFrameRate, /*abs_error=*/0.5);
}

TEST_F(VideoTrackAdapterFixtureTest, StreamWithJitterFrameRateReduction) {
  const double kInputFrameRate = 10.0;
  const double kTargetFrameRate = 5.0;
  const int kNumFrames = 1000;

  // Creates a 10 fps timestamp series: 0, 10 ms, 200 ms, 210 ms, ...
  auto index_to_timestamp =
      base::BindLambdaForTesting([kTargetFrameRate](int i) {
        return (i / 2) * base::Seconds(1.0 / kTargetFrameRate) +
               (i % 2) * base::Seconds(0.05 / kTargetFrameRate);
      });
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, kTargetFrameRate, index_to_timestamp);

  EXPECT_EQ(num_delivered + num_dropped, kNumFrames);
  EXPECT_NEAR(static_cast<double>(num_delivered) / kNumFrames * kInputFrameRate,
              kTargetFrameRate, /*abs_error=*/0.5);
}

TEST_F(VideoTrackAdapterFixtureTest, DoNotDropFramesIfFrameRatesMatch) {
  const int kNumFrames = 1000;
  auto [num_delivered, num_dropped] = GenerateAndCountFrames(
      kNumFrames, /*target_frame_rate=*/5.0, /*actual_input_frame_rate=*/5.0);
  EXPECT_EQ(num_delivered, kNumFrames);
  EXPECT_EQ(num_dropped, 0);
}

TEST_F(VideoTrackAdapterFixtureTest, FrameRateReductionSlightMismatch) {
  const double kInputFrameRate = 10.4;
  const double kTargetFrameRate = 10.0;
  const int kNumFrames = 1000;
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, kInputFrameRate, kTargetFrameRate);
  EXPECT_EQ(num_delivered + num_dropped, kNumFrames);
  EXPECT_NEAR(static_cast<double>(num_delivered) / kNumFrames * kInputFrameRate,
              kTargetFrameRate, /*abs_error=*/0.5);
}

TEST_F(VideoTrackAdapterFixtureTest, DoNotDropFramesIfNoTargetFrameRate) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, /*target_frame_rate=*/std::nullopt,
                             /*actual_input_frame_rate=*/10.0);
  EXPECT_EQ(num_delivered, kNumFrames);
  EXPECT_EQ(num_dropped, 0);
}

TEST_F(VideoTrackAdapterFixtureTest, DropFramesIfTargetFrameRateIsInfinite) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] = GenerateAndCountFrames(
      kNumFrames, /*target_frame_rate=*/std::numeric_limits<double>::infinity(),
      /*actual_input_frame_rate=*/10.0);
  EXPECT_EQ(num_delivered, kNumFrames);
  EXPECT_EQ(num_dropped, 0);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DoNotDropFramesIfTimeBetweenFramesIsNegative) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, /*target_frame_rate=*/10.0,
                             /*actual_input_frame_rate=*/-10.0);
  EXPECT_EQ(num_delivered, kNumFrames);
  EXPECT_EQ(num_dropped, 0);
}

TEST_F(VideoTrackAdapterFixtureTest,
       DoNotDropFramesIfTimeBetweenFramesIsTooLong) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, /*target_frame_rate=*/10.0,
                             /*actual_input_frame_rate=*/0.0000001);
  EXPECT_EQ(num_delivered, kNumFrames);
  EXPECT_EQ(num_dropped, 0);
}

TEST_F(VideoTrackAdapterFixtureTest, DropFramesIfTimeBetweenFramesIsTooShort) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] =
      GenerateAndCountFrames(kNumFrames, /*target_frame_rate=*/10.0,
                             /*actual_input_frame_rate=*/10000000.0);
  EXPECT_EQ(num_delivered, 1);
  EXPECT_EQ(num_dropped, kNumFrames - 1);
}

TEST_F(VideoTrackAdapterFixtureTest, DropFramesIfTimeBetweenFramesIsZero) {
  const int kNumFrames = 100;
  auto [num_delivered, num_dropped] = GenerateAndCountFrames(
      kNumFrames, /*target_frame_rate=*/10.0,
      /*actual_input_frame_rate=*/std::numeric_limits<double>::infinity());
  EXPECT_EQ(num_delivered, 1);
  EXPECT_EQ(num_dropped, kNumFrames - 1);
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
        false /* remote */, std::move(source));
    RunSyncOnRenderThread([&] {
      adapter_ = base::MakeRefCounted<VideoTrackAdapter>(
          platform_support_->GetIOTaskRunner(), mock_source_->GetWeakPtr());
    });
  }

  ~VideoTrackAdapterEncodedTest() override {
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
          base::DoNothing(),
          base::BindRepeating(
              &VideoTrackAdapterEncodedTest::OnEncodedVideoFrameDelivered,
              base::Unretained(this)),
          /*sub_capture_target_version_callback=*/base::DoNothing(),
          /*settings_callback=*/base::DoNothing(),
          /*track_callback=*/base::DoNothing(), VideoTrackAdapterSettings());
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
  test::TaskEnvironment task_environment_;
  base::Thread render_thread_;
  WebMediaStreamSource web_source_;
  raw_ptr<MockMediaStreamVideoSource, DanglingUntriaged> mock_source_;
  scoped_refptr<VideoTrackAdapter> adapter_;
};

TEST_F(VideoTrackAdapterEncodedTest, DeliverEncodedVideoFrame) {
  auto track1 = AddTrack();
  auto track2 = AddTrack();
  EXPECT_CALL(*this, OnEncodedVideoFrameDelivered).Times(2);

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  platform_support_->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        adapter_->DeliverEncodedVideoFrameOnVideoTaskRunner(
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
