// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/capture_scheduler.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "remoting/proto/video.pb.h"

namespace {

// Number of samples to average the most recent capture and encode time
// over.
const int kStatisticsWindow = 3;

// The hard limit is 30fps or 33ms per recording cycle.
const int64_t kDefaultMinimumIntervalMs = 33;

// Controls how much CPU time we can use for encode and capture.
// Range of this value is between 0 to 1. 0 means using 0% of of all CPUs
// available while 1 means using 100% of all CPUs available.
const double kRecordingCpuConsumption = 0.5;

// Maximum number of captured frames in the encoding queue. Currently capturer
// implementations do not allow to keep more than 2 DesktopFrame objects.
static const int kMaxFramesInEncodingQueue = 2;

// Maximum number of unacknowledged frames. Ignored if the client doesn't
// support ACKs. This value was chosen experimentally, using synthetic
// performance tests (see ProtocolPerfTest), to maximize frame rate, while
// keeping round-trip latency low.
static const int kMaxUnacknowledgedFrames = 4;

}  // namespace

namespace remoting::protocol {

// We assume that the number of available cores is constant.
CaptureScheduler::CaptureScheduler(
    const base::RepeatingClosure& capture_closure)
    : capture_closure_(capture_closure),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      capture_timer_(new base::OneShotTimer()),
      minimum_interval_(base::Milliseconds(kDefaultMinimumIntervalMs)),
      num_of_processors_(base::SysInfo::NumberOfProcessors()),
      capture_time_(kStatisticsWindow),
      encode_time_(kStatisticsWindow),
      num_encoding_frames_(0),
      num_unacknowledged_frames_(0),
      capture_pending_(false),
      is_paused_(false),
      next_frame_id_(0) {
  DCHECK(num_of_processors_);
}

CaptureScheduler::~CaptureScheduler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CaptureScheduler::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScheduleNextCapture();
}

void CaptureScheduler::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_paused_ != pause) {
    is_paused_ = pause;

    if (is_paused_) {
      capture_timer_->Stop();
    } else {
      ScheduleNextCapture();
    }
  }
}

void CaptureScheduler::OnCaptureCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_pending_ = false;
  capture_time_.Record(
      (tick_clock_->NowTicks() - last_capture_started_time_).InMilliseconds());

  ++num_encoding_frames_;

  ScheduleNextCapture();
}

void CaptureScheduler::OnFrameEncoded(VideoPacket* packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Set packet_id for the outgoing packet.
  packet->set_frame_id(next_frame_id_);
  ++next_frame_id_;

  // Update internal stats.
  encode_time_.Record(packet->encode_time_ms());

  --num_encoding_frames_;
  ++num_unacknowledged_frames_;

  ScheduleNextCapture();
}

void CaptureScheduler::OnFrameSent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScheduleNextCapture();
}

void CaptureScheduler::ProcessVideoAck(std::unique_ptr<VideoAck> video_ack) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  --num_unacknowledged_frames_;
  DCHECK_GE(num_unacknowledged_frames_, 0);

  ScheduleNextCapture();
}

void CaptureScheduler::SetTickClockForTest(const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void CaptureScheduler::SetTimerForTest(
    std::unique_ptr<base::OneShotTimer> timer) {
  capture_timer_ = std::move(timer);
}

void CaptureScheduler::SetNumOfProcessorsForTest(int num_of_processors) {
  num_of_processors_ = num_of_processors;
}

void CaptureScheduler::ScheduleNextCapture() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_paused_ || capture_pending_ ||
      num_encoding_frames_ >= kMaxFramesInEncodingQueue) {
    return;
  }

  if (num_encoding_frames_ + num_unacknowledged_frames_ >=
      kMaxUnacknowledgedFrames) {
    return;
  }

  // Delay by an amount chosen such that if capture and encode times
  // continue to follow the averages, then we'll consume the target
  // fraction of CPU across all cores.
  base::TimeDelta delay = std::max(
      minimum_interval_,
      base::Milliseconds((capture_time_.Average() + encode_time_.Average()) /
                         (kRecordingCpuConsumption * num_of_processors_)));

  // Account for the time that has passed since the last capture.
  delay = std::max(base::TimeDelta(), delay - (tick_clock_->NowTicks() -
                                               last_capture_started_time_));

  capture_timer_->Start(FROM_HERE, delay,
                        base::BindOnce(&CaptureScheduler::CaptureNextFrame,
                                       base::Unretained(this)));
}

void CaptureScheduler::CaptureNextFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_paused_);
  DCHECK(!capture_pending_);

  capture_pending_ = true;
  last_capture_started_time_ = tick_clock_->NowTicks();
  capture_closure_.Run();
}

}  // namespace remoting::protocol
