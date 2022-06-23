// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopSize;

namespace remoting {
namespace protocol {

class WebrtcFrameSchedulerTest : public ::testing::Test {
 public:
  WebrtcFrameSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        frame_(DesktopSize(1, 1)) {}
  ~WebrtcFrameSchedulerTest() override = default;

  void InitConstantRateScheduler() {
    scheduler_ = std::make_unique<WebrtcFrameSchedulerConstantRate>();
    scheduler_->Start(base::BindRepeating(
        &WebrtcFrameSchedulerTest::CaptureCallback, base::Unretained(this)));
  }

  void CaptureCallback() {
    capture_callback_count_++;

    if (simulate_capture_) {
      // Simulate a completed capture and encode.
      scheduler_->OnFrameCaptured(&frame_);
      WebrtcVideoEncoder::EncodedFrame encoded_frame;
      encoded_frame.key_frame = false;
      encoded_frame.data = webrtc::EncodedImageBuffer::Create(1);
      scheduler_->OnFrameEncoded(WebrtcVideoEncoder::EncodeResult::SUCCEEDED,
                                 &encoded_frame);
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<WebrtcFrameScheduler> scheduler_;

  int capture_callback_count_ = 0;
  bool simulate_capture_ = true;
  BasicDesktopFrame frame_;
};

TEST_F(WebrtcFrameSchedulerTest, NoCapturesIfZeroFps) {
  InitConstantRateScheduler();
  scheduler_->SetMaxFramerateFps(0);

  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(0, capture_callback_count_);
}

TEST_F(WebrtcFrameSchedulerTest, CapturesAtRequestedFramerate) {
  InitConstantRateScheduler();
  scheduler_->SetMaxFramerateFps(60);

  task_environment_.FastForwardBy(base::Seconds(1));

  // There should be approximately 60 captures in 1 second, making an allowance
  // for any off-by-one artifacts in timing.
  EXPECT_LE(59, capture_callback_count_);
  EXPECT_LE(capture_callback_count_, 61);
}

TEST_F(WebrtcFrameSchedulerTest, NoCaptureWhileCapturePending) {
  InitConstantRateScheduler();
  simulate_capture_ = false;
  scheduler_->SetMaxFramerateFps(60);

  task_environment_.FastForwardBy(base::Seconds(1));

  // Only 1 capture callback, because the fake "capturer" never returns a
  // captured frame. The scheduler should only do 1 capture at a time.
  EXPECT_EQ(1, capture_callback_count_);
}

TEST_F(WebrtcFrameSchedulerTest, NoCaptureWhilePaused) {
  InitConstantRateScheduler();
  scheduler_->SetMaxFramerateFps(60);
  scheduler_->Pause(true);

  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(0, capture_callback_count_);

  scheduler_->Pause(false);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_LE(1, capture_callback_count_);
}

}  // namespace protocol
}  // namespace remoting
