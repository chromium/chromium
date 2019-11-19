// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
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

constexpr base::TimeDelta kVirtualTestDurationSeconds =
    base::TimeDelta::FromSeconds(100);

// The value of the padding bytes in unpacked frames.
const uint8_t kFramePaddingValue = 0;

// Use a special value for frame pixels to tell pixel bytes apart from the
// padding bytes in the unpacked frame test.
const uint8_t kFakePixelValue = 1;

// Use a special value for the first pixel to verify the result in the inverted
// frame test.
const uint8_t kFakePixelValueFirst = 2;

// Creates a DesktopFrame that has the first pixel bytes set to
// kFakePixelValueFirst, and the rest of the bytes set to kFakePixelValue, for
// UnpackedFrame and InvertedFrame verification.
std::unique_ptr<webrtc::BasicDesktopFrame> CreateBasicFrame(
    const webrtc::DesktopSize& size) {
  std::unique_ptr<webrtc::BasicDesktopFrame> frame(
      new webrtc::BasicDesktopFrame(size));
  DCHECK_EQ(frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel,
            frame->stride());
  memset(frame->data(), kFakePixelValue,
         frame->stride() * frame->size().height());
  memset(frame->data(), kFakePixelValueFirst,
         webrtc::DesktopFrame::kBytesPerPixel);
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
            frame->data() + (frame->size().height() - 1) * frame->stride(),
            frame->shared_memory()) {
    set_dpi(frame->dpi());
    set_capture_time_ms(frame->capture_time_ms());
    mutable_updated_region()->Swap(frame->mutable_updated_region());
    original_frame_ = std::move(frame);
  }
  ~InvertedDesktopFrame() override {}

 private:
  std::unique_ptr<webrtc::DesktopFrame> original_frame_;

  DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

// DesktopFrame wrapper that copies the input frame and doubles the stride.
class UnpackedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit UnpackedDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame)
      : webrtc::DesktopFrame(
            frame->size(),
            frame->stride() * 2,
            new uint8_t[frame->stride() * 2 * frame->size().height()],
            nullptr) {
    memset(data(), kFramePaddingValue, stride() * size().height());
    CopyPixelsFrom(*frame, webrtc::DesktopVector(),
                   webrtc::DesktopRect::MakeSize(size()));
  }
  ~UnpackedDesktopFrame() override {
    delete[] data_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnpackedDesktopFrame);
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public webrtc::DesktopCapturer {
 public:
  FakeScreenCapturer()
      : callback_(nullptr),
        frame_index_(0),
        generate_inverted_frames_(false),
        generate_cropped_frames_(false) {}
  ~FakeScreenCapturer() override {}

  void set_generate_inverted_frames(bool generate_inverted_frames) {
    generate_inverted_frames_ = generate_inverted_frames;
  }

  void set_generate_cropped_frames(bool generate_cropped_frames) {
    generate_cropped_frames_ = generate_cropped_frames;
  }

  // VideoFrameCapturer interface.
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    webrtc::DesktopSize size;
    if (frame_index_ % 2 == 0) {
      size = webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1);
    } else {
      size = webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2);
    }
    frame_index_++;

    std::unique_ptr<webrtc::DesktopFrame> frame = CreateBasicFrame(size);

    if (generate_inverted_frames_) {
      frame.reset(new InvertedDesktopFrame(std::move(frame)));
    } else if (generate_cropped_frames_) {
      frame.reset(new UnpackedDesktopFrame(std::move(frame)));
    }
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* screens) override { return false; }

  bool SelectSource(SourceId id) override { return false; }

 private:
  Callback* callback_;
  int frame_index_;
  bool generate_inverted_frames_;
  bool generate_cropped_frames_;
};

// Helper used to check that only two specific frame sizes are delivered to the
// OnIncomingCapturedData() callback.
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
    EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);
  }

 private:
  const gfx::Size size_for_even_frames_;
  const gfx::Size size_for_odd_frames_;
  int frame_count_;
};

}  // namespace

class DesktopCaptureDeviceTest : public testing::Test {
 public:
  void CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer> capturer) {
    capture_device_.reset(new DesktopCaptureDevice(
        std::move(capturer), DesktopMediaID::TYPE_SCREEN));
  }

  void CopyFrame(const uint8_t* frame,
                 int size,
                 const media::VideoCaptureFormat&,
                 const gfx::ColorSpace&,
                 int /* clockwise_rotation */,
                 bool /* flip_y */,
                 base::TimeTicks /* reference_time */,
                 base::TimeDelta /* timestamp */,
                 int /* frame_feedback_id */) {
    ASSERT_TRUE(output_frame_);
    ASSERT_EQ(output_frame_->stride() * output_frame_->size().height(), size);
    memcpy(output_frame_->data(), frame, size);
  }

 protected:
  std::unique_ptr<media::MockVideoCaptureDeviceClient>
  CreateMockVideoCaptureDeviceClient() {
    auto result = std::make_unique<media::MockVideoCaptureDeviceClient>();
    ON_CALL(*result, ReserveOutputBuffer(_, _, _, _))
        .WillByDefault(Invoke([](const gfx::Size&,
                                 media::VideoPixelFormat format, int,
                                 media::VideoCaptureDevice::Client::Buffer*) {
          EXPECT_TRUE(format == media::PIXEL_FORMAT_I420);
          return media::VideoCaptureDevice::Client::ReserveResult::kSucceeded;
        }));
    return result;
  }

  std::unique_ptr<DesktopCaptureDevice> capture_device_;
  std::unique_ptr<webrtc::DesktopFrame> output_frame_;
};

// There is currently no screen capturer implementation for ozone. So disable
// the test that uses a real screen-capturer instead of FakeScreenCapturer.
// http://crbug.com/260318
#if defined(USE_OZONE)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
TEST_F(DesktopCaptureDeviceTest, MAYBE_Capture) {
  std::unique_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::DesktopCapturer::CreateScreenCapturer(
          webrtc::DesktopCaptureOptions::CreateDefault()));
  CreateScreenCaptureDevice(std::move(capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int frame_size;

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(SaveArg<1>(&frame_size), SaveArg<2>(&format),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

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
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);

  EXPECT_EQ(format.frame_size.GetArea() * 4, frame_size);
}

// Test that screen capturer behaves correctly if the source frame size changes
// but the caller cannot cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeConstantResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth1, kTestFrameHeight1));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(WithArg<2>(Invoke(&format_checker,
                                  &FormatChecker::ExpectAcceptableSize)),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

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
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes,
// where the video frames sent the the client vary in resolution but maintain
// the same aspect ratio.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeFixedAspectRatio) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(888, 500), gfx::Size(532, 300));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(WithArg<2>(Invoke(&format_checker,
                                  &FormatChecker::ExpectAcceptableSize)),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

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
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// Test that screen capturer behaves correctly if the source frame size changes
// and the caller can cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeVariableResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer>(mock_capturer));

  FormatChecker format_checker(gfx::Size(kTestFrameWidth1, kTestFrameHeight1),
                               gfx::Size(kTestFrameWidth2, kTestFrameHeight2));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(WithArg<2>(Invoke(&format_checker,
                                  &FormatChecker::ExpectAcceptableSize)),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

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
  // OnIncomingCapturedData() will use FormatChecker to examine the format of
  // each frame being delivered.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
    done_event.Reset();
  }

  capture_device_->StopAndDeAllocate();
}

// This test verifies that an unpacked frame is converted to a packed frame.
TEST_F(DesktopCaptureDeviceTest, UnpackedFrame) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();
  mock_capturer->set_generate_cropped_frames(true);
  CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  int frame_size = 0;
  output_frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1)));

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(Invoke(this, &DesktopCaptureDeviceTest::CopyFrame),
                SaveArg<1>(&frame_size),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

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

  // Verifies that |output_frame_| has the same data as a packed frame of the
  // same size.
  std::unique_ptr<webrtc::BasicDesktopFrame> expected_frame = CreateBasicFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1));
  EXPECT_EQ(output_frame_->stride() * output_frame_->size().height(),
            frame_size);
  EXPECT_EQ(
      0, memcmp(output_frame_->data(), expected_frame->data(), frame_size));
}

// The test verifies that a bottom-to-top frame is converted to top-to-bottom.
TEST_F(DesktopCaptureDeviceTest, InvertedFrame) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();
  mock_capturer->set_generate_inverted_frames(true);
  CreateScreenCaptureDevice(
      std::unique_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  int frame_size = 0;
  output_frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1)));

  std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
      CreateMockVideoCaptureDeviceClient());
  EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*client, OnStarted());
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(Invoke(this, &DesktopCaptureDeviceTest::CopyFrame),
                SaveArg<1>(&frame_size),
                InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, std::move(client));

  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  capture_device_->StopAndDeAllocate();

  // Verifies that |output_frame_| has the same pixel values as the inverted
  // frame.
  std::unique_ptr<webrtc::DesktopFrame> inverted_frame(
      new InvertedDesktopFrame(CreateBasicFrame(
          webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1))));
  EXPECT_EQ(output_frame_->stride() * output_frame_->size().height(),
            frame_size);
  for (int i = 0; i < output_frame_->size().height(); ++i) {
    EXPECT_EQ(0,
        memcmp(inverted_frame->data() + i * inverted_frame->stride(),
               output_frame_->data() + i * output_frame_->stride(),
               output_frame_->stride()));
  }
}

class DesktopCaptureDeviceThrottledTest : public DesktopCaptureDeviceTest {
 public:
  // Capture frames at kFrameRate for a duration of total_capture_duration and
  // return the throttled frame rate.
  double CaptureFrames() {
    CreateScreenCaptureDevice(
        std::unique_ptr<webrtc::DesktopCapturer>(new FakeScreenCapturer()));

    FormatChecker format_checker(
        gfx::Size(kTestFrameWidth3, kTestFrameHeight3),
        gfx::Size(kTestFrameWidth3, kTestFrameHeight3));

    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    scoped_refptr<base::SingleThreadTaskRunner> message_loop_task_runner;
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner;
    int nb_frames = 0;

    std::unique_ptr<media::MockVideoCaptureDeviceClient> client(
        CreateMockVideoCaptureDeviceClient());
    EXPECT_CALL(*client, OnError(_, _, _)).Times(0);
    // On started is called from the capture thread.
    EXPECT_CALL(*client, OnStarted())
        .WillOnce(InvokeWithoutArgs([this, &task_runner,
                                     &message_loop_task_runner] {
          message_loop_task_runner = base::ThreadTaskRunnerHandle::Get();
          task_runner = new base::TestMockTimeTaskRunner(
              base::Time::Now(), base::TimeTicks::Now(),
              base::TestMockTimeTaskRunner::Type::kStandalone);
          capture_device_->SetMockTimeForTesting(
              task_runner, task_runner->GetMockTickClock());
        }));

    EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
        .WillRepeatedly(DoAll(
            WithArg<2>(
                Invoke(&format_checker, &FormatChecker::ExpectAcceptableSize)),
            WithArg<7>(Invoke([&done_event, &nb_frames, &task_runner,
                               &message_loop_task_runner](
                                  base::TimeDelta timestamp) {
              ++nb_frames;

              // Simulate real device capture time. Indeed the time spent
              // here in OnIncomingCapturedData is take into account for
              // the capture duration
              const base::TimeDelta device_capture_duration =
                  base::TimeDelta::FromMicroseconds(static_cast<int64_t>(
                      static_cast<double>(base::Time::kMicrosecondsPerSecond) /
                          kFrameRate +
                      0.5 /* round to nearest int */));
              task_runner->FastForwardBy(device_capture_duration);

              // Stop advancing the virtual time when reaching the end.
              if (timestamp > kVirtualTestDurationSeconds) {
                done_event.Signal();
              } else {
                // 'PostNonNestable' is required to make sure the next one
                // shot capture timer is already pushed when forwaring the
                // virtual time by the next pending task delay.
                message_loop_task_runner->PostNonNestableTask(
                    FROM_HERE,
                    base::BindOnce(
                        [](scoped_refptr<base::TestMockTimeTaskRunner>
                               task_runner) {
                          task_runner->FastForwardBy(
                              task_runner->NextPendingTaskDelay());
                        },
                        task_runner));
              }
            }))));
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

    return nb_frames / kVirtualTestDurationSeconds.InSecondsF();
  }
};

// The test verifies that the capture pipeline is throttled as defined with
// kDefaultMaximumCpuConsumptionPercentage.
TEST_F(DesktopCaptureDeviceThrottledTest, ThrottledOn) {
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

// The test verifies that the capture pipeline is not throttled when passing
// --webrtc-max-cpu-consumption-percentage=100.
TEST_F(DesktopCaptureDeviceThrottledTest, ThrottledOff) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--webrtc-max-cpu-consumption-percentage", "100");

  const double actual_framerate = CaptureFrames();

  // Throttling is disabled so the test expects the configured framerate.
  const int expected_framerate = kFrameRate;

  // The test succeeds if the actual framerate is near the expected_framerate.
  EXPECT_GE(actual_framerate, expected_framerate);
  EXPECT_LE(actual_framerate, expected_framerate + 0.1);
}

// The test verifies that the capture pipeline is throttled when passing
// --webrtc-max-cpu-consumption-percentage=80.
TEST_F(DesktopCaptureDeviceThrottledTest, Throttled80) {
  const int max_cpu_consumption_percentage = 80;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--webrtc-max-cpu-consumption-percentage",
      base::NumberToString(max_cpu_consumption_percentage));

  const double actual_framerate = CaptureFrames();

  // The pipeline is throttled to ensure that the cpu is idle for at least
  // N% of the time.
  const int expected_framerate =
      (kFrameRate * max_cpu_consumption_percentage) / 100;

  // The test succeeds if the actual framerate is near the expected_framerate.
  EXPECT_GE(actual_framerate, expected_framerate);
  EXPECT_LE(actual_framerate, expected_framerate + 0.1);
}

}  // namespace content
