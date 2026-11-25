// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/test/browser_task_environment.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/media_switches.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/frame_texture.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::WithArg;

namespace content {

namespace {

const int kTestFrameWidth1 = 500;
const int kTestFrameHeight1 = 500;
const int kTestFrameWidth2 = 400;
const int kTestFrameHeight2 = 300;
const int kTestFrameWidth3 = 64;
const int kTestFrameHeight3 = 64;

const int kFrameRate = 30;

constexpr base::TimeDelta kVirtualTestDurationSeconds = base::Seconds(3);

// The value of the padding bytes in unpacked frames.
const uint8_t kFramePaddingValue = 0;

// Use a special value for frame pixels to tell pixel bytes apart from the
// padding bytes in the unpacked frame test.
const uint8_t kFakePixelValue = 1;

// Use a special value for the first pixel to verify the result in the inverted
// frame test.
const uint8_t kFakePixelValueFirst = 2;

const char kFrameIsRefresh[] = "WebRTC.DesktopCapture.FrameIsRefresh.Screen";

// Creates a DesktopFrame that has the first pixel bytes set to
// kFakePixelValueFirst, and the rest of the bytes set to kFakePixelValue, for
// UnpackedFrame and InvertedFrame verification.
// The complete frame is marked as updated by default independently of size,
// position and content to ensure that the frame is not marked as "not changed"
// by the DesktopCaptureDevice since that would prevent the frame from being
// forwarded to the client.
// See DesktopCapturerDifferWrapperTest for a more realistic example of how the
// content of frames should affect the updated region part of each frame.
std::unique_ptr<webrtc::BasicDesktopFrame> CreateBasicFrame(
    const webrtc::DesktopSize& size,
    webrtc::FourCC pixel_format = webrtc::FOURCC_ARGB) {
  auto frame = std::make_unique<webrtc::BasicDesktopFrame>(size, pixel_format);
  DCHECK_EQ(frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel,
            frame->stride());
  UNSAFE_TODO(memset(frame->data(), kFakePixelValue,
                     frame->stride() * frame->size().height()));
  UNSAFE_TODO(memset(frame->data(), kFakePixelValueFirst,
                     webrtc::DesktopFrame::kBytesPerPixel));
  frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeSize(size));
  return frame;
}

// DesktopFrame wrapper that flips wrapped frame upside down by inverting
// stride.
class InvertedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit InvertedDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame)
      : webrtc::DesktopFrame(
            frame->size(),
            -frame->stride(),
            frame->pixel_format(),
            UNSAFE_TODO(frame->data() +
                        (frame->size().height() - 1) * frame->stride()),
            frame->shared_memory()) {
    set_dpi(frame->dpi());
    set_capture_time_ms(frame->capture_time_ms());
    set_device_scale_factor(frame->device_scale_factor());
    mutable_updated_region()->Swap(frame->mutable_updated_region());
    original_frame_ = std::move(frame);
  }

  InvertedDesktopFrame(const InvertedDesktopFrame&) = delete;
  InvertedDesktopFrame& operator=(const InvertedDesktopFrame&) = delete;

  ~InvertedDesktopFrame() override {}

 private:
  std::unique_ptr<webrtc::DesktopFrame> original_frame_;
};

// DesktopFrame wrapper that copies the input frame and doubles the stride.
class UnpackedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit UnpackedDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame)
      : webrtc::DesktopFrame(
            frame->size(),
            frame->stride() * 2,
            frame->pixel_format(),
            new uint8_t[frame->stride() * 2 * frame->size().height()],
            nullptr) {
    set_device_scale_factor(frame->device_scale_factor());
    UNSAFE_TODO(memset(data(), kFramePaddingValue, stride() * size().height()));
    CopyPixelsFrom(*frame, webrtc::DesktopVector(),
                   webrtc::DesktopRect::MakeSize(size()));
  }

  UnpackedDesktopFrame(const UnpackedDesktopFrame&) = delete;
  UnpackedDesktopFrame& operator=(const UnpackedDesktopFrame&) = delete;

  ~UnpackedDesktopFrame() override {
    delete[] data_;
  }
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public webrtc::DesktopCapturer {
 public:
  FakeScreenCapturer() = default;
  ~FakeScreenCapturer() override = default;

  void set_generate_inverted_frames(bool generate_inverted_frames) {
    generate_inverted_frames_ = generate_inverted_frames;
  }

  void set_generate_cropped_frames(bool generate_cropped_frames) {
    generate_cropped_frames_ = generate_cropped_frames;
  }

  void set_run_callback_asynchronously(bool run_callback_asynchronously) {
    run_callback_asynchronously_ = run_callback_asynchronously;
  }

  void set_generate_non_updated_frames(bool generate_non_updated_frames,
                                       int no_update_period) {
    generate_non_updated_frames_ = generate_non_updated_frames;
    no_update_period_ = no_update_period;
  }

  void set_pixel_format(webrtc::FourCC pixel_format) {
    pixel_format_ = pixel_format;
  }

  // DesktopCapturer interface.
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    webrtc::DesktopSize size;
    if (generate_non_updated_frames_) {
      size = webrtc::DesktopSize(kTestFrameWidth3, kTestFrameHeight3);
    } else if (captured_frames_ % 2 == 0) {
      size = webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1);
    } else {
      size = webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2);
    }
    captured_frames_++;

    std::unique_ptr<webrtc::DesktopFrame> frame =
        CreateBasicFrame(size, pixel_format_);
    frame->set_device_scale_factor(2.0f);
    if (generate_non_updated_frames_ &&
        captured_frames_ % no_update_period_ == 0) {
      // Indicates that no region of the screen has been updated since the last
      // captured frame. Frame size and content is ignored to simplify testing.
      frame->mutable_updated_region()->Clear();
    }

    if (generate_inverted_frames_) {
      frame = std::make_unique<InvertedDesktopFrame>(std::move(frame));
    } else if (generate_cropped_frames_) {
      frame = std::make_unique<UnpackedDesktopFrame>(std::move(frame));
    }

    if (run_callback_asynchronously_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&FakeScreenCapturer::RunCallback,
                                    weak_factory_.GetWeakPtr(),
                                    webrtc::DesktopCapturer::Result::SUCCESS,
                                    std::move(frame)));
    } else {
      callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                                 std::move(frame));
    }
  }

  int captured_frames() const { return captured_frames_; }

  bool GetSourceList(SourceList* screens) override { return false; }

  bool SelectSource(SourceId id) override { return false; }

 private:
  void RunCallback(webrtc::DesktopCapturer::Result result,
                   std::unique_ptr<webrtc::DesktopFrame> frame) {
    callback_->OnCaptureResult(result, std::move(frame));
  }

  raw_ptr<Callback> callback_ = nullptr;
  int captured_frames_ = 0;
  bool generate_inverted_frames_ = false;
  bool generate_cropped_frames_ = false;
  bool run_callback_asynchronously_ = false;
  // Every |no_update_period_| frame will have an empty updated region if
  // this member is true.
  bool generate_non_updated_frames_ = false;
  int no_update_period_ = std::numeric_limits<int>::max();
  webrtc::FourCC pixel_format_ = webrtc::FOURCC_ARGB;
  base::WeakPtrFactory<FakeScreenCapturer> weak_factory_{this};
};

// Helper used to check that only two specific frame sizes are delivered to the
// DoOnIncomingCapturedBufferExt() callback.
class FormatChecker {
 public:
  FormatChecker(const gfx::Size& size_for_even_frames,
                const gfx::Size& size_for_odd_frames)
      : size_for_even_frames_(size_for_even_frames),
        size_for_odd_frames_(size_for_odd_frames),
        frame_count_(0) {}

  void ExpectAcceptableSize(const media::VideoCaptureFormat& format) {
    if (frame_count_ % 2 == 0)
      EXPECT_EQ(size_for_even_frames_, format.frame_size);
    else
      EXPECT_EQ(size_for_odd_frames_, format.frame_size);
    ++frame_count_;
    EXPECT_EQ(kFrameRate, format.frame_rate);
  }

 private:
  const gfx::Size size_for_even_frames_;
  const gfx::Size size_for_odd_frames_;
  int frame_count_;
};

}  // namespace

class DesktopCaptureDeviceTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(media::kZeroCopyDesktopCapture,
                                              GetParam());
  }

  void CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer> capturer) {
    capture_device_.reset(new DesktopCaptureDevice(
        std::move(capturer), DesktopMediaID::TYPE_SCREEN));
  }

  void CopyFrame(media::VideoCaptureDevice::Client::Buffer& buffer,
                 const media::VideoCaptureFormat& format,
                 const gfx::ColorSpace& color_space,
                 base::TimeTicks reference_time,
                 base::TimeDelta timestamp,
                 gfx::Rect visible_rect,
                 const std::optional<media::VideoFrameMetadata>& metadata) {
    ASSERT_TRUE(output_frame_);
    ASSERT_NE(metadata, std::nullopt);
    ASSERT_EQ(metadata->capture.source_size->width(),
              output_frame_->size().width());
    ASSERT_EQ(metadata->capture.source_size->height(),
              output_frame_->size().height());
    CopyFrameScaled(buffer, format, color_space, reference_time, timestamp,
                    visible_rect, metadata);
  }

  void CopyFrameScaled(
      media::VideoCaptureDevice::Client::Buffer& buffer,
      const media::VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const std::optional<media::VideoFrameMetadata>& metadata) {
    ASSERT_TRUE(output_frame_);
    auto handle = buffer.handle_provider->GetHandleForInProcessAccess();

    // Ensure size fits for I420.
    size_t expected_size = media::VideoFrame::AllocationSize(
        format.pixel_format, format.frame_size);
    ASSERT_GE(handle->mapped_size(), expected_size);
    ASSERT_EQ(format.frame_size.width(), output_frame_->size().width());
    ASSERT_EQ(format.frame_size.height(), output_frame_->size().height());
    ASSERT_EQ(format.pixel_format, media::PIXEL_FORMAT_I420);
    ASSERT_NE(metadata, std::nullopt);
    ASSERT_NE(metadata->capture.source_size, std::nullopt);
    ASSERT_EQ(metadata->device_scale_factor, 2.0f);

    // SAFETY: output_frame_ is allocated with the same dimensions and format
    // so it is guaranteed to hold expected_size bytes.
    UNSAFE_BUFFERS(base::span<uint8_t>(output_frame_->data(), expected_size))
        .copy_from(handle->data().first(expected_size));
  }

  void CopyFrameLegacy(base::span<const uint8_t> frame,
                       const media::VideoCaptureFormat& format,
                       const gfx::ColorSpace& color_space,
                       int clockwise_rotation,
                       bool flip_y,
                       base::TimeTicks reference_time,
                       base::TimeDelta timestamp,
                       std::optional<base::TimeTicks> capture_begin_time,
                       const std::optional<media::VideoFrameMetadata>& metadata,
                       int frame_feedback_id) {
    ASSERT_TRUE(output_frame_);
    ASSERT_NE(metadata, std::nullopt);
    ASSERT_EQ(metadata->capture.source_size->width(),
              output_frame_->size().width());
    ASSERT_EQ(metadata->capture.source_size->height(),
              output_frame_->size().height());
    CopyFrameScaledLegacy(frame, format, color_space, clockwise_rotation,
                          flip_y, reference_time, timestamp, capture_begin_time,
                          metadata, frame_feedback_id);
  }

  void CopyFrameScaledLegacy(
      base::span<const uint8_t> frame,
      const media::VideoCaptureFormat& format,
      const gfx::ColorSpace&,
      int /* clockwise_rotation */,
      bool /* flip_y */,
      base::TimeTicks /* reference_time */,
      base::TimeDelta /* timestamp */,
      std::optional<base::TimeTicks> /* capture_begin_time */,
      const std::optional<media::VideoFrameMetadata>& metadata,
      int /* frame_feedback_id */) {
    ASSERT_TRUE(output_frame_);
    ASSERT_EQ(static_cast<size_t>(output_frame_->stride() *
                                  output_frame_->size().height()),
              frame.size());
    ASSERT_EQ(format.frame_size.width(), output_frame_->size().width());
    ASSERT_EQ(format.frame_size.height(), output_frame_->size().height());
    ASSERT_EQ(format.pixel_format,
              FourCCToVideoPixelFormat(output_frame_->pixel_format()));
    ASSERT_NE(metadata, std::nullopt);
    ASSERT_NE(metadata->capture.source_size, std::nullopt);
    ASSERT_EQ(metadata->device_scale_factor, 2.0f);
    // SAFETY: output_frame_ is allocated with the same size.
    UNSAFE_BUFFERS(base::span<uint8_t>(output_frame_->data(), frame.size()))
        .copy_from(frame);
  }

 protected:
  std::unique_ptr<media::MockVideoCaptureDeviceClient>
  CreateMockVideoCaptureDeviceClient() {
    auto result = media::MockVideoCaptureDeviceClient::
        CreateMockClientWithBufferAllocator(
            base::BindRepeating([](const media::VideoCaptureFormat&) {}));
    return result;
  }

  void ExpectCapture(media::MockVideoCaptureDeviceClient* client,
                     media::VideoCaptureFormat* format,
                     base::WaitableEvent* done_event) {
    if (GetParam()) {
      EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
          .WillRepeatedly(DoAll(
              SaveArg<1>(format),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    } else {
      EXPECT_CALL(*client, OnIncomingCapturedData)
          .WillRepeatedly(DoAll(
              SaveArg<1>(format),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    }
  }

  void ExpectCaptureFormatChecker(media::MockVideoCaptureDeviceClient* client,
                                  FormatChecker* format_checker,
                                  base::WaitableEvent* done_event,
                                  int expected_count = -1) {
    if (GetParam()) {
      auto& call = EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt);
      if (expected_count >= 0) {
        call.Times(expected_count);
      }
      call.WillRepeatedly(
          DoAll(WithArg<1>(Invoke(format_checker,
                                  &FormatChecker::ExpectAcceptableSize)),
                InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    } else {
      auto& call = EXPECT_CALL(*client, OnIncomingCapturedData);
      if (expected_count >= 0) {
        call.Times(expected_count);
      }
      call.WillRepeatedly(
          DoAll(WithArg<1>(Invoke(format_checker,
                                  &FormatChecker::ExpectAcceptableSize)),
                InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    }
  }

  void ExpectCaptureCopyFrame(media::MockVideoCaptureDeviceClient* client,
                              base::WaitableEvent* done_event) {
    if (GetParam()) {
      EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
          .WillRepeatedly(DoAll(
              Invoke(this, &DesktopCaptureDeviceTest::CopyFrame),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    } else {
      EXPECT_CALL(*client, OnIncomingCapturedData)
          .WillRepeatedly(DoAll(
              Invoke(this, &DesktopCaptureDeviceTest::CopyFrameLegacy),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    }
  }

  void ExpectCaptureCopyFrameScaled(media::MockVideoCaptureDeviceClient* client,
                                    base::WaitableEvent* done_event) {
    if (GetParam()) {
      EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
          .WillOnce(DoAll(
              Invoke(this, &DesktopCaptureDeviceTest::CopyFrameScaled),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    } else {
      EXPECT_CALL(*client, OnIncomingCapturedData)
          .WillOnce(DoAll(
              Invoke(this, &DesktopCaptureDeviceTest::CopyFrameScaledLegacy),
              InvokeWithoutArgs(done_event, &base::WaitableEvent::Signal)));
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<DesktopCaptureDevice> capture_device_;
  std::unique_ptr<webrtc::DesktopFrame> output_frame_;
};

// Capturer implementation for Fuchsia is not fully functional.
// TODO(crbug.com/445218901): Capturer implementation for Android needs user
// input to work.
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
TEST_P(DesktopCaptureDeviceTest, Capture) {
  std::unique_ptr<webrtc::DesktopCapturer> capturer(
      desktop_capture::CreateScreenCapturer(
          webrtc::DesktopCaptureOptions::CreateDefault(),
          /*for_snapshot=*/false));

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !BUILDFLAG(SUPPORTS_OZONE_X11)
  // webrtc::DesktopCapturer is only supported on Ozone X11 by default.
  // TODO(webrtc/13429): Enable for Wayland.
  EXPECT_FALSE(capturer);
  GTEST_SKIP();
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // !BUILDFLAG(SUPPORTS_OZONE_X11)

  EXPECT_TRUE(capturer);

  CreateScreenCaptureDevice(std::move(capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCapture(client.get(), &format, &done_event);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_device_->AllocateAndStart(capture_params, std::move(client));
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device_->StopAndDeAllocate();

  EXPECT_GT(format.frame_size.width(), 0);
  EXPECT_GT(format.frame_size.height(), 0);
  EXPECT_EQ(kFrameRate, format.frame_rate);
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)

// Test that screen capturer behaves correctly if the source frame size changes
// but the caller cannot cope with variable resolution output.
TEST_P(DesktopCaptureDeviceTest, ScreenResolutionChangeConstantResolution) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth1, kTestFrameHeight1));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureFormatChecker(client.get(), &format_checker, &done_event);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_RESOLUTION;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least two frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // DoOnIncomingCapturedBufferExt() will use FormatChecker to examine the
  // format of each frame being delivered.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes,
// where the video frames sent the the client vary in resolution but maintain
// the same aspect ratio.
TEST_P(DesktopCaptureDeviceTest, ScreenResolutionChangeFixedAspectRatio) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  FormatChecker format_checker(gfx::Size(888, 500), gfx::Size(532, 300));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureFormatChecker(client.get(), &format_checker, &done_event);

  media::VideoCaptureParams capture_params;
  const gfx::Size high_def_16_by_9(1920, 1080);
  ASSERT_GE(high_def_16_by_9.width(),
            std::max(kTestFrameWidth1, kTestFrameWidth2));
  ASSERT_GE(high_def_16_by_9.height(),
            std::max(kTestFrameHeight1, kTestFrameHeight2));
  capture_params.requested_format.frame_size = high_def_16_by_9;
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least three frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // DoOnIncomingCapturedBufferExt() will use FormatChecker to examine the
  // format of each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes
// and the caller can cope with variable resolution output.
TEST_P(DesktopCaptureDeviceTest, ScreenResolutionChangeVariableResolution) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth2, kTestFrameHeight2));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureFormatChecker(client.get(), &format_checker, &done_event);

  media::VideoCaptureParams capture_params;
  const gfx::Size high_def_16_by_9(1920, 1080);
  ASSERT_GE(high_def_16_by_9.width(),
            std::max(kTestFrameWidth1, kTestFrameWidth2));
  ASSERT_GE(high_def_16_by_9.height(),
            std::max(kTestFrameHeight1, kTestFrameHeight2));
  capture_params.requested_format.frame_size = high_def_16_by_9;
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_params.resolution_change_policy =
      media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Capture at least three frames, to ensure that the source frame size has
  // changed to two different sizes while capturing.  The mock for
  // DoOnIncomingCapturedBufferExt() will use FormatChecker to examine the
  // format of each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// This test verifies that an unpacked frame is converted to a packed frame.
TEST_P(DesktopCaptureDeviceTest, UnpackedFrame) {
  auto mock_capturer = std::make_unique<FakeScreenCapturer>();
  mock_capturer->set_generate_cropped_frames(true);
  CreateScreenCaptureDevice(std::move(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  output_frame_ = std::make_unique<webrtc::BasicDesktopFrame>(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1),
      GetParam() ? webrtc::FOURCC_I420 : webrtc::FOURCC_ARGB);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureCopyFrame(client.get(), &done_event);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format =
      media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();
}

// The test verifies that a bottom-to-top frame is converted to top-to-bottom.
TEST_P(DesktopCaptureDeviceTest, InvertedFrame) {
  auto mock_capturer = std::make_unique<FakeScreenCapturer>();
  mock_capturer->set_generate_inverted_frames(true);
  CreateScreenCaptureDevice(std::move(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  output_frame_ = std::make_unique<webrtc::BasicDesktopFrame>(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1),
      GetParam() ? webrtc::FOURCC_I420 : webrtc::FOURCC_ARGB);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureCopyFrame(client.get(), &done_event);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();
}

// Test that a capturer that produces ABGR frames is handled correctly when the
// output frame size is different and scaling is required.
TEST_P(DesktopCaptureDeviceTest, CaptureAbgrFrameWithScaling) {
  auto capturer = std::make_unique<FakeScreenCapturer>();
  capturer->set_pixel_format(webrtc::FOURCC_ABGR);
  CreateScreenCaptureDevice(std::move(capturer));

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // The output frame will be I420 because scaling is required.
  output_frame_ = std::make_unique<webrtc::BasicDesktopFrame>(
      webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2),
      webrtc::FOURCC_I420);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureCopyFrameScaled(client.get(), &done_event);

  media::VideoCaptureParams capture_params;
  // Request a different frame size to trigger scaling.
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth2,
                                                     kTestFrameHeight2);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device_->StopAndDeAllocate();
}

// This test verifies that calling RequestRefreshFrame() on the screen capturer
// before AllocateAndStart() does not provide any refresh frame.
TEST_P(DesktopCaptureDeviceTest, RequestRefreshFrameBeforeStart) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted).Times(0);
  if (GetParam()) {
    EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt).Times(0);
  } else {
    EXPECT_CALL(*client, OnIncomingCapturedData).Times(0);
  }

  capture_device_->RequestRefreshFrame();
  capture_device_->StopAndDeAllocate();
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 0);
}

// This test verifies that calling RequestRefreshFrame() on the screen capturer
// after StopAndDeAllocate() does not result in any refresh frame even if one
// frame has been captured before StopAndDeAllocate() was called.
TEST_P(DesktopCaptureDeviceTest, RequestRefreshFrameAfterStop) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  if (GetParam()) {
    EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
        .Times(1)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  } else {
    EXPECT_CALL(*client, OnIncomingCapturedData)
        .Times(1)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  }

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  // AllocateAndStart() should trigger one call to
  // DoOnIncomingCapturedBufferExt() but RequestRefreshFrame() should not
  // trigger a second call to DoOnIncomingCapturedBufferExt() since it is is
  // called after StopAndDeAllocate();
  capture_device_->AllocateAndStart(capture_params, std::move(client));
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, false, 1);
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 1);
  capture_device_->RequestRefreshFrame();
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 1);
}

// Verify that calling RequestRefreshFrame() results in an extra frame being
// captured and sent to the client. The content should not be the same as for
// the first default frame.
TEST_P(DesktopCaptureDeviceTest, RequestRefreshFrameSendsExtraFrame) {
  CreateScreenCaptureDevice(std::make_unique<FakeScreenCapturer>());

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth1, kTestFrameHeight1));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnStarted);
  ExpectCaptureFormatChecker(client.get(), &format_checker, &done_event, 2);

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));
  capture_device_->RequestRefreshFrame();

  // Capture two frames; one default (the first) and one refresh (the second).
  // The mock for DoOnIncomingCapturedBufferExt() will use FormatChecker to
  // examine the format of each frame being delivered.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, false, 1);
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device_->StopAndDeAllocate();
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, true, 1);
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 2);
}

// Verifies that only captured frames which contains updated regions are
// forwarded to the client. In reality such a "no change" event should be an
// effect of static size, content and position of the frame, but to allow for a
// less complex verification, frames are here periodically marked as
// "not updated" independently of its own content or the content of the previous
// frame.
TEST_P(DesktopCaptureDeviceTest,
       OnlyCapturedFramesWithUpdatedRegionsAreForwardedToTheClient) {
  auto capturer = std::make_unique<FakeScreenCapturer>();
  FakeScreenCapturer* mock_capturer = capturer.get();
  // Marks captured frame #2, #4, etc. (first frame is #1) as not updated.
  mock_capturer->set_generate_non_updated_frames(true, 2);
  CreateScreenCaptureDevice(std::move(capturer));

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Drive two frames to the registered client. A non-forwarded captured frame
  // due to "no-change" is silently discarded by the VideoCaptureDevice but the
  // rate of capturing new frames is not affected.
  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnFrameDropped).Times(0);
  if (GetParam()) {
    EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
        .Times(2)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  } else {
    EXPECT_CALL(*client, OnIncomingCapturedData)
        .Times(2)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  }

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth3,
                                                     kTestFrameHeight3);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Ensure that the client gets two captured frames but the capturer had to
  // capture three frames to do so since frame #2 is marked as "not updated".
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(mock_capturer->captured_frames(), 3);

  capture_device_->StopAndDeAllocate();
}

// Verifies that RequestRefreshFrame() forces a new captured frame even when the
// content has not changed since the last frame. This is to ensure that the
// client gets a new frame when explicitly asking for it.
TEST_P(DesktopCaptureDeviceTest,
       RequestRefreshFrameSendsFrameEvenIfNoRegionsAreUpdated) {
  auto capturer = std::make_unique<FakeScreenCapturer>();
  FakeScreenCapturer* mock_capturer = capturer.get();
  // Marks captured frame #2, #4, etc. (first frame is #1) as not updated.
  mock_capturer->set_generate_non_updated_frames(true, 2);
  CreateScreenCaptureDevice(std::move(capturer));

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Drive three frames to the registered client. A non-forwarded captured frame
  // due to "no-change" is silently discarded by the VideoCaptureDevice but the
  // rate of capturing new frames is not affected.
  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnFrameDropped).Times(0);
  if (GetParam()) {
    EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
        .Times(3)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  } else {
    EXPECT_CALL(*client, OnIncomingCapturedData)
        .Times(3)
        .WillRepeatedly(
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));
  }

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth3,
                                                     kTestFrameHeight3);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  // Ensure that the client gets two captured frames but the capturer had to
  // capture three frames to do so since frame #2 is marked as "not updated".
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  }
  EXPECT_EQ(mock_capturer->captured_frames(), 3);
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, false, 3);
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 3);

  // Next frame is #4 and it will be marked as "not updated". Asking for a
  // refresh at this point in time should override the default
  // "0Hz functionality" and forward the new refresh frame.
  capture_device_->RequestRefreshFrame();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(mock_capturer->captured_frames(), 4);
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, true, 1);
  histogram_tester_.ExpectBucketCount(kFrameIsRefresh, false, 3);
  histogram_tester_.ExpectTotalCount(kFrameIsRefresh, 4);

  capture_device_->StopAndDeAllocate();
}

class DesktopCaptureDeviceThrottledTest : public DesktopCaptureDeviceTest {
 public:
  // Capture frames at kFrameRate for a duration of total_capture_duration and
  // return the throttled frame rate.
  double CaptureFrames() {
    auto capturer = std::make_unique<FakeScreenCapturer>();
    capturer->set_run_callback_asynchronously(run_callback_asynchronously_);

    CreateScreenCaptureDevice(std::move(capturer));

    FormatChecker format_checker(
        gfx::Size(kTestFrameWidth3, kTestFrameHeight3),
        gfx::Size(kTestFrameWidth3, kTestFrameHeight3));

    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    scoped_refptr<base::SingleThreadTaskRunner> message_loop_task_runner;
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner;
    int nb_frames = 0;
    base::TimeDelta final_timestamp;

    std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
        CreateMockVideoCaptureDeviceClient());
    EXPECT_CALL(*client, OnError).Times(0);
    // On started is called from the capture thread.
    EXPECT_CALL(*client, OnStarted)
        .WillOnce([this, &task_runner, &message_loop_task_runner] {
          message_loop_task_runner =
              base::SingleThreadTaskRunner::GetCurrentDefault();
          task_runner = new base::TestMockTimeTaskRunner(
              base::Time::Now(), base::TimeTicks::Now(),
              base::TestMockTimeTaskRunner::Type::kStandalone);
          capture_device_->SetMockTimeForTesting(
              task_runner, task_runner->GetMockTickClock());
        });

    auto on_frame = [&done_event, &nb_frames, &task_runner,
                     &message_loop_task_runner,
                     &final_timestamp](base::TimeDelta timestamp) {
      ++nb_frames;

      // Simulate real device capture time. Indeed the time spent
      // here in DoOnIncomingCapturedBufferExt/OnIncomingCapturedData is taken
      // into account for the capture duration
      const base::TimeDelta device_capture_duration =
          base::Microseconds(static_cast<int64_t>(
              static_cast<double>(base::Time::kMicrosecondsPerSecond) /
                  kFrameRate +
              0.5 /* round to nearest int */));
      task_runner->FastForwardBy(device_capture_duration);

      // Stop advancing the virtual time when reaching the end.
      if (timestamp > kVirtualTestDurationSeconds) {
        final_timestamp = timestamp;
        done_event.Signal();
      } else {
        // 'PostNonNestable' is required to make sure the next one
        // shot capture timer is already pushed when forwarding the
        // virtual time by the next pending task delay.
        message_loop_task_runner->PostNonNestableTask(
            FROM_HERE,
            base::BindOnce(
                [](scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
                  task_runner->FastForwardBy(
                      task_runner->NextPendingTaskDelay());
                },
                task_runner));
      }
    };

    if (GetParam()) {
      EXPECT_CALL(*client, DoOnIncomingCapturedBufferExt)
          .WillRepeatedly(
              DoAll(WithArg<1>(Invoke(&format_checker,
                                      &FormatChecker::ExpectAcceptableSize)),
                    WithArg<4>(on_frame)));
    } else {
      EXPECT_CALL(*client, OnIncomingCapturedData)
          .WillRepeatedly(
              DoAll(WithArg<1>(Invoke(&format_checker,
                                      &FormatChecker::ExpectAcceptableSize)),
                    WithArg<6>(on_frame)));
    }
    media::VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size.SetSize(kTestFrameWidth3,
                                                       kTestFrameHeight3);
    capture_params.requested_format.frame_rate = kFrameRate;
    capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
    capture_params.resolution_change_policy =
        media::ResolutionChangePolicy::FIXED_RESOLUTION;

    capture_device_->AllocateAndStart(capture_params, std::move(client));

    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();

    EXPECT_GT(nb_frames, 0);

    capture_device_->StopAndDeAllocate();

    return (nb_frames - 1) / final_timestamp.InSecondsF();
  }

  bool run_callback_asynchronously_ = false;
};

// The test verifies that the capture pipeline is throttled as defined with
// kDefaultMaximumCpuConsumptionPercentage.
TEST_P(DesktopCaptureDeviceThrottledTest, ThrottledOn) {
  const double actual_framerate = CaptureFrames();

  // By default when capturing a frame it is expected to do the actual device
  // capture for at most half of a capture period. This is to ensure that the
  // cpu is idle for at least 50% of the time, otherwise it will be throttled
  // to reach this idle duration.
  const int expected_framerate = kFrameRate / 2;

  // The test succeeds if the actual framerate is near the expected_framerate.
  EXPECT_GE(actual_framerate, expected_framerate);
  EXPECT_LE(actual_framerate, expected_framerate + 0.1);
}

// Same tests as above but runs callbacks asynchronously to verify that that
// doesn't disrupt the throttling machinery.
TEST_P(DesktopCaptureDeviceThrottledTest, ThrottledOn_Async) {
  run_callback_asynchronously_ = true;

  const double actual_framerate = CaptureFrames();

  // By default when capturing a frame it is expected to do the actual device
  // capture for at most half of a capture period. This is to ensure that the
  // cpu is idle for at least 50% of the time, otherwise it will be throttled
  // to reach this idle duration.
  const int expected_framerate = kFrameRate / 2;

  // The test succeeds if the actual framerate is near the expected_framerate.
  EXPECT_GE(actual_framerate, expected_framerate);
  EXPECT_LE(actual_framerate, expected_framerate + 0.1);
}

INSTANTIATE_TEST_SUITE_P(All, DesktopCaptureDeviceTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         DesktopCaptureDeviceThrottledTest,
                         testing::Bool());

namespace {

class FakeFrameTexture : public webrtc::FrameTexture {
 public:
  FakeFrameTexture()
#if BUILDFLAG(IS_WIN)
      : webrtc::FrameTexture(CreateEvent(nullptr, FALSE, FALSE, nullptr)),
        owned_handle_(handle())
#else
      : webrtc::FrameTexture(1)
#endif
  {
  }
  ~FakeFrameTexture() override = default;

 private:
#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle owned_handle_;
#endif
};

class TextureDesktopFrame : public webrtc::DesktopFrame {
 public:
  explicit TextureDesktopFrame(const webrtc::DesktopSize& size)
      : TextureDesktopFrame(size, std::make_unique<FakeFrameTexture>()) {}

  ~TextureDesktopFrame() override = default;

 private:
  TextureDesktopFrame(const webrtc::DesktopSize& size,
                      std::unique_ptr<FakeFrameTexture> texture)
      : webrtc::DesktopFrame(size,
                             /*stride=*/0,
                             webrtc::FOURCC_ARGB,
                             /*data=*/nullptr,
                             /*shared_memory=*/nullptr,
                             texture.get()),
        owned_texture_(std::move(texture)) {
    mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeSize(size));
  }

  std::unique_ptr<FakeFrameTexture> owned_texture_;
};

class FakeTextureCapturer : public webrtc::DesktopCapturer {
 public:
  FakeTextureCapturer() = default;
  ~FakeTextureCapturer() override = default;

  void set_max_texture_frames(int max_texture_frames) {
    max_texture_frames_.store(max_texture_frames, std::memory_order_release);
  }

  void WaitForCompletedTextureFrames(int target_count) {
    base::AutoLock lock(lock_);
    while (completed_texture_frames_ < target_count) {
      cv_.Wait();
    }
  }

  int capture_attempts() {
    base::AutoLock lock(lock_);
    return capture_attempts_;
  }

  // webrtc::DesktopCapturer:
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    const int max_frames = max_texture_frames_.load(std::memory_order_acquire);
    {
      base::AutoLock lock(lock_);
      ++capture_attempts_;
      if (max_frames >= 0 && completed_texture_frames_ >= max_frames) {
        base::AutoUnlock unlock(lock_);
        callback_->OnCaptureResult(
            webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
        return;
      }
    }

    auto frame = std::make_unique<TextureDesktopFrame>(
        webrtc::DesktopSize(kTestFrameWidth3, kTestFrameHeight3));
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));

    base::AutoLock lock(lock_);
    ++completed_texture_frames_;
    cv_.Broadcast();
  }

  bool GetSourceList(SourceList* screens) override { return false; }
  bool SelectSource(SourceId id) override { return false; }

 private:
  raw_ptr<Callback> callback_ = nullptr;
  std::atomic<int> max_texture_frames_{-1};
  base::Lock lock_;
  base::ConditionVariable cv_{&lock_};
  int capture_attempts_ GUARDED_BY(lock_) = 0;
  int completed_texture_frames_ GUARDED_BY(lock_) = 0;
};

}  // namespace

class DesktopCaptureDeviceTextureTest : public testing::Test {
 protected:
  void CreateWindowCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer> capturer) {
    capture_device_.reset(new DesktopCaptureDevice(
        std::move(capturer), DesktopMediaID::TYPE_WINDOW));

    // Bind a dummy WakeLock pipe so Core::AllocateAndStart() does not spin up
    // DeviceService / Linux D-Bus threads in unit tests.
    mojo::PendingRemote<device::mojom::WakeLock> dummy_wake_lock;
    wake_lock_receiver_ = dummy_wake_lock.InitWithNewPipeAndPassReceiver();
    capture_device_->SetWakeLockForTesting(std::move(dummy_wake_lock));

#if BUILDFLAG(IS_WIN)
    capture_device_->SetGpuLuidForTesting(
        GpuDataManager::GetInstance()->GetGPUInfo().active_gpu().luid);
#endif
  }

  void SetSharedImageInterfaceGetter(
      DesktopCaptureDevice::SharedImageInterfaceGetter getter) {
    capture_device_->SetSharedImageInterfaceGetterForTesting(std::move(getter));
  }

#if BUILDFLAG(IS_WIN)
  void SetGpuLuid(CHROME_LUID luid) {
    capture_device_->SetGpuLuidForTesting(luid);
  }
#endif

  void FlushCaptureThread() {
    base::RunLoop loop;
    capture_device_->thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), loop.QuitClosure());
    loop.Run();
  }

  media::VideoCaptureParams CaptureParamsWithFrameRate(float frame_rate) const {
    media::VideoCaptureParams params;
    params.requested_format.frame_size.SetSize(kTestFrameWidth3,
                                               kTestFrameHeight3);
    params.requested_format.frame_rate = frame_rate;
    params.requested_format.pixel_format = media::PIXEL_FORMAT_ARGB;
    return params;
  }

  media::VideoCaptureParams DefaultCaptureParams() const {
    return CaptureParamsWithFrameRate(kFrameRate);
  }

  BrowserTaskEnvironment task_environment_;
  mojo::PendingReceiver<device::mojom::WakeLock> wake_lock_receiver_;
  std::unique_ptr<DesktopCaptureDevice> capture_device_;
};

// Verifies that texture frames arriving before the UI-thread
// SharedImageInterface request completes are dropped without reporting OnError,
// and that stopping the device while the UI-thread request is still in flight
// safely drops the reply.
TEST_F(DesktopCaptureDeviceTextureTest,
       DropsFramesWhileSharedImageInterfacePendingAndStopsCleanly) {
  auto capturer = std::make_unique<FakeTextureCapturer>();
  FakeTextureCapturer* raw_capturer = capturer.get();
  CreateWindowCaptureDevice(std::move(capturer));

  base::RunLoop ui_getter_loop;
  bool getter_ran_on_ui = false;
  SetSharedImageInterfaceGetter(base::BindLambdaForTesting(
      [&]() -> scoped_refptr<gpu::SharedImageInterface> {
        EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
        getter_ran_on_ui = true;
        ui_getter_loop.Quit();
        return base::MakeRefCounted<gpu::TestSharedImageInterface>();
      }));

  auto client =
      std::make_unique<NiceMock<media::MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnError).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedImage).Times(0);
  EXPECT_CALL(*client, OnStarted);

  capture_device_->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  // Wait for two texture frames on the capture thread before pumping the UI
  // thread message loop, ensuring SharedImageInterface creation is still
  // pending.
  raw_capturer->WaitForCompletedTextureFrames(2);
  EXPECT_FALSE(getter_ran_on_ui);

  // Stop the device before the UI thread processes the request; running the UI
  // thread getter afterwards must safely drop the reply to the destroyed Core.
  capture_device_->StopAndDeAllocate();
  ui_getter_loop.Run();
  EXPECT_TRUE(getter_ran_on_ui);
}

// Verifies that when UI-thread SharedImageInterface creation fails (e.g. no GPU
// channel is available), frames while pending are dropped without error and the
// subsequent texture frame reports OnError.
TEST_F(DesktopCaptureDeviceTextureTest,
       ReportsErrorWhenSharedImageInterfaceUnavailable) {
  auto capturer = std::make_unique<FakeTextureCapturer>();
  FakeTextureCapturer* raw_capturer = capturer.get();
  CreateWindowCaptureDevice(std::move(capturer));

  base::RunLoop error_loop;
  auto quit_error_loop = error_loop.QuitClosure();
  std::atomic<bool> error_reported{false};

  auto client =
      std::make_unique<NiceMock<media::MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(*client, OnIncomingCapturedImage).Times(0);
  EXPECT_CALL(
      *client,
      OnError(media::VideoCaptureError::kDesktopCaptureDeviceGpuAdapterChanged,
              _, _))
      .WillOnce([&] {
        error_reported.store(true, std::memory_order_release);
        quit_error_loop.Run();
      });

  // Uses the default production UI-thread getter; in unit tests
  // BrowserGpuChannelHostFactory::instance() is null so it returns nullptr,
  // and the next texture frame triggers OnError.
  capture_device_->AllocateAndStart(DefaultCaptureParams(), std::move(client));

  raw_capturer->WaitForCompletedTextureFrames(1);
  EXPECT_FALSE(error_reported.load(std::memory_order_acquire));

  error_loop.Run();
  EXPECT_TRUE(error_reported.load(std::memory_order_acquire));
  capture_device_->StopAndDeAllocate();
}

// Verifies that OnSharedImageInterfaceCreated() immediately triggers a refresh
// capture when ready, and that if the SharedImageInterface later becomes lost
// (simulating a GPU process crash), texture delivery reports OnError.
TEST_F(DesktopCaptureDeviceTextureTest,
       ReportsErrorWhenSharedImageInterfaceLost) {
  auto capturer = std::make_unique<FakeTextureCapturer>();
  FakeTextureCapturer* raw_capturer = capturer.get();
  raw_capturer->set_max_texture_frames(1);
  CreateWindowCaptureDevice(std::move(capturer));

  base::RunLoop ui_getter_loop;
  auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  SetSharedImageInterfaceGetter(base::BindLambdaForTesting(
      [&]() -> scoped_refptr<gpu::SharedImageInterface> {
        EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
        ui_getter_loop.Quit();
        return test_sii;
      }));

  base::RunLoop error_loop;
  auto quit_error_loop = error_loop.QuitClosure();
  std::atomic<bool> error_reported{false};

  auto client =
      std::make_unique<NiceMock<media::MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(
      *client,
      OnError(media::VideoCaptureError::kDesktopCaptureDeviceGpuAdapterChanged,
              _, _))
      .WillOnce([&] {
        error_reported.store(true, std::memory_order_release);
        quit_error_loop.Run();
      });

  // Request 1 fps so the periodic timer will not fire for 1000 ms; the second
  // capture attempt must come from OnSharedImageInterfaceCreated() calling
  // CaptureFrame(/*is_refresh_frame=*/true).
  capture_device_->AllocateAndStart(CaptureParamsWithFrameRate(1.0f),
                                    std::move(client));

  raw_capturer->WaitForCompletedTextureFrames(1);
  EXPECT_EQ(raw_capturer->capture_attempts(), 1);
  EXPECT_FALSE(error_reported.load(std::memory_order_acquire));

  ui_getter_loop.Run();
  FlushCaptureThread();
  EXPECT_EQ(raw_capturer->capture_attempts(), 2);
  EXPECT_FALSE(error_reported.load(std::memory_order_acquire));

  test_sii->MarkAsLost();
  raw_capturer->set_max_texture_frames(-1);
  capture_device_->RequestRefreshFrame();

  error_loop.Run();
  EXPECT_TRUE(error_reported.load(std::memory_order_acquire));
  capture_device_->StopAndDeAllocate();
}

#if BUILDFLAG(IS_WIN)
// Verifies end-to-end texture delivery once the UI-thread SharedImageInterface
// arrives (via immediate refresh capture at 1 fps), followed by OnError when
// the SharedImageInterface becomes lost.
TEST_F(DesktopCaptureDeviceTextureTest,
       DeliversTextureOnceSharedImageInterfaceReadyAndErrorsOnLoss) {
  auto capturer = std::make_unique<FakeTextureCapturer>();
  FakeTextureCapturer* raw_capturer = capturer.get();
  CreateWindowCaptureDevice(std::move(capturer));

  auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  SetSharedImageInterfaceGetter(base::BindLambdaForTesting(
      [&]() -> scoped_refptr<gpu::SharedImageInterface> {
        EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
        return test_sii;
      }));

  base::RunLoop image_loop;
  auto quit_image_loop = image_loop.QuitClosure();
  base::RunLoop error_loop;
  auto quit_error_loop = error_loop.QuitClosure();

  auto client =
      std::make_unique<NiceMock<media::MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(*client, OnIncomingCapturedImage)
      .WillOnce([&] { quit_image_loop.Run(); })
      .WillRepeatedly([] {});
  EXPECT_CALL(
      *client,
      OnError(media::VideoCaptureError::kDesktopCaptureDeviceGpuAdapterChanged,
              _, _))
      .WillOnce([&] { quit_error_loop.Run(); });

  capture_device_->AllocateAndStart(CaptureParamsWithFrameRate(1.0f),
                                    std::move(client));

  // Frame 1 is dropped while the UI-thread request is pending.
  raw_capturer->WaitForCompletedTextureFrames(1);

  // Pumping the UI thread delivers SharedImageInterface and immediately
  // triggers a refresh capture delivering OnIncomingCapturedImage.
  image_loop.Run();

  // Simulating a GPU process crash causes OnError on the next frame.
  test_sii->MarkAsLost();
  capture_device_->RequestRefreshFrame();
  error_loop.Run();

  capture_device_->StopAndDeAllocate();
}

// Verifies that if the GPU adapter LUID changes while the UI-thread
// SharedImageInterface request is still in flight (transitioning sii_state_ to
// kFailed), the late reply in OnSharedImageInterfaceCreated() is ignored
// cleanly without hitting a CHECK.
TEST_F(DesktopCaptureDeviceTextureTest,
       IgnoresSharedImageInterfaceReplyAfterGpuLuidChanged) {
  auto capturer = std::make_unique<FakeTextureCapturer>();
  FakeTextureCapturer* raw_capturer = capturer.get();
  CreateWindowCaptureDevice(std::move(capturer));

  base::RunLoop ui_getter_loop;
  auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  SetSharedImageInterfaceGetter(base::BindLambdaForTesting(
      [&]() -> scoped_refptr<gpu::SharedImageInterface> {
        EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
        ui_getter_loop.Quit();
        return test_sii;
      }));

  base::WaitableEvent error_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto client =
      std::make_unique<NiceMock<media::MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(*client, OnIncomingCapturedImage).Times(0);
  EXPECT_CALL(
      *client,
      OnError(media::VideoCaptureError::kDesktopCaptureDeviceGpuAdapterChanged,
              _, _))
      .WillOnce(InvokeWithoutArgs(&error_event, &base::WaitableEvent::Signal));

  capture_device_->AllocateAndStart(CaptureParamsWithFrameRate(1.0f),
                                    std::move(client));

  // Frame 1 sets sii_state_ = kPending while the UI-thread task has not run.
  raw_capturer->WaitForCompletedTextureFrames(1);

  // Simulate an adapter LUID mismatch before the UI-thread reply arrives.
  SetGpuLuid(CHROME_LUID{0xdead, 0xbeef});
  capture_device_->RequestRefreshFrame();
  EXPECT_TRUE(error_event.TimedWait(TestTimeouts::action_max_timeout()));

  // Now run the UI-thread getter and flush its reply to Core while
  // sii_state_ == kFailed.
  ui_getter_loop.Run();
  FlushCaptureThread();

  capture_device_->StopAndDeAllocate();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
