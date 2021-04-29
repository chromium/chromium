// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
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
      : task_runner_(
            // Default ctor starts clock with null TimeTicks, which confuses
            // the scheduler, so use the current time as a baseline.
            new base::TestMockTimeTaskRunner(base::Time::Now(),
                                             base::TimeTicks::Now())),
        task_runner_handle_(task_runner_.get()),
        frame_(DesktopSize(1, 1)) {
    scheduler_ = std::make_unique<WebrtcFrameSchedulerSimple>(SessionOptions());
    scheduler_->SetTickClockForTest(task_runner_->GetMockTickClock());
    scheduler_->Start(
        base::BindRepeating(&WebrtcFrameSchedulerTest::CaptureCallback,
                            base::Unretained(this)));
    scheduler_->OnEncoderReady();
  }
  ~WebrtcFrameSchedulerTest() override = default;

  void CaptureCallback() {
    capture_callback_count_++;

    if (simulate_capture_) {
      // Simulate a completed capture and encode.
      WebrtcVideoEncoder::FrameParams out_params;
      scheduler_->OnFrameCaptured(&frame_, &out_params);
      WebrtcVideoEncoder::EncodedFrame encoded;
      encoded.key_frame = out_params.key_frame;
      encoded.data = 'X';
      scheduler_->OnFrameEncoded(WebrtcVideoEncoder::EncodeResult::SUCCEEDED,
                                 &encoded);
      scheduler_->GetSchedulerStats(frame_stats_);
    }
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  std::unique_ptr<WebrtcFrameSchedulerSimple> scheduler_;

  int capture_callback_count_ = 0;
  bool simulate_capture_ = false;
  BasicDesktopFrame frame_;
  HostFrameStats frame_stats_;
};

TEST_F(WebrtcFrameSchedulerTest, UpdateBitrateWhenPending) {
  scheduler_->OnKeyFrameRequested();
  scheduler_->OnTargetBitrateChanged(100);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1, capture_callback_count_);

  scheduler_->OnTargetBitrateChanged(1001);

  // |task_runner_| shouldn't have pending tasks as the scheduler should be
  // waiting for the previous capture request to complete.
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(WebrtcFrameSchedulerTest, EmptyFrameUpdate_ShouldNotBeSentImmediately) {
  // Needed to avoid DCHECK in OnFrameCaptured().
  scheduler_->OnTargetBitrateChanged(100);

  WebrtcVideoEncoder::FrameParams out_params;

  // Initial capture, full frame.
  frame_.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));
  scheduler_->OnFrameCaptured(&frame_, &out_params);
  // Empty frame.
  frame_.mutable_updated_region()->Clear();
  bool result = scheduler_->OnFrameCaptured(&frame_, &out_params);

  // Should not be sent, because of throttling of empty frames.
  EXPECT_FALSE(result);
}

TEST_F(WebrtcFrameSchedulerTest, EmptyFrameUpdate_ShouldBeSentAfter2000ms) {
  // Identical to the previous test, except it waits a short amount of time
  // before the empty frame update.
  scheduler_->OnTargetBitrateChanged(100);

  WebrtcVideoEncoder::FrameParams out_params;

  // Initial capture, full frame.
  frame_.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));
  scheduler_->OnFrameCaptured(&frame_, &out_params);
  // Wait more than 2000ms.
  task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(3000));
  // Empty frame.
  frame_.mutable_updated_region()->Clear();
  bool result = scheduler_->OnFrameCaptured(&frame_, &out_params);

  // Empty frames should be sent at the throttled rate.
  EXPECT_TRUE(result);
}

TEST_F(WebrtcFrameSchedulerTest, Capturer_RunsAt30Fps) {
  simulate_capture_ = true;

  scheduler_->OnTargetBitrateChanged(100);

  // Have the capturer return non-empty frames each time.
  frame_.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));

  // Ensure the encoder is ready, otherwise the scheduler will not trigger
  // repeated captures.
  scheduler_->OnKeyFrameRequested();

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // There should be approximately 30 captures in 1 second.
  EXPECT_LE(29, capture_callback_count_);
  EXPECT_LE(capture_callback_count_, 31);
}

TEST_F(WebrtcFrameSchedulerTest, RttReportedInFrameStats) {
  simulate_capture_ = true;
  scheduler_->OnKeyFrameRequested();
  scheduler_->OnTargetBitrateChanged(100);
  frame_.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));
  auto rtt = base::TimeDelta::FromMilliseconds(123);
  scheduler_->OnRttUpdate(rtt);

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(rtt, frame_stats_.rtt_estimate);
}

}  // namespace protocol
}  // namespace remoting
