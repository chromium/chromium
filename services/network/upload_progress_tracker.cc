// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/upload_progress_tracker.h"

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/upload_progress.h"
#include "net/url_request/url_request.h"

namespace network {
namespace {
// The interval for calls to ReportUploadProgress.
constexpr base::TimeDelta kUploadProgressInterval = base::Milliseconds(100);
}  // namespace

UploadProgressTracker::UploadProgressTracker(
    const base::Location& location,
    UploadProgressReportCallback report_progress,
    net::URLRequest* request,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : request_(request), report_progress_(std::move(report_progress)) {
  DCHECK(report_progress_);

  progress_timer_.SetTaskRunner(std::move(task_runner));
  progress_timer_.Start(location, kUploadProgressInterval, this,
                        &UploadProgressTracker::ReportUploadProgressIfNeeded);
}

UploadProgressTracker::~UploadProgressTracker() {}

void UploadProgressTracker::OnAckReceived() {
  waiting_for_upload_progress_ack_ = false;
}

void UploadProgressTracker::OnUploadCompleted() {
  waiting_for_upload_progress_ack_ = false;
  ReportUploadProgressIfNeeded();
  progress_timer_.Stop();
}

// static
base::TimeDelta UploadProgressTracker::GetUploadProgressIntervalForTesting() {
  return kUploadProgressInterval;
}

base::TimeTicks UploadProgressTracker::GetCurrentTime() const {
  return base::TimeTicks::Now();
}

net::UploadProgress UploadProgressTracker::GetUploadProgress() const {
  return request_->GetUploadProgress();
}

void UploadProgressTracker::ReportUploadProgressIfNeeded() {
  if (waiting_for_upload_progress_ack_)
    return;

  net::UploadProgress progress = GetUploadProgress();
  if (!progress.size())
    return;  // Nothing to upload, or in the chunked upload mode.

  // No progress made since last time, or the progress was reset by a redirect
  // or a retry.
  if (progress.position() <= last_upload_position_)
    return;

  const uint64_t kHalfPercentIncrements = 200;
  const base::TimeDelta kOneSecond = base::Milliseconds(1000);

  uint64_t amt_since_last = progress.position() - last_upload_position_;
  base::TimeTicks now = GetCurrentTime();
  base::TimeDelta time_since_last = now - last_upload_ticks_;

  bool is_finished = (progress.size() == progress.position());
  bool enough_new_progress =
      (amt_since_last > (progress.size() / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    report_progress_.Run(progress);
    waiting_for_upload_progress_ack_ = true;
    last_upload_ticks_ = now;
    last_upload_position_ = progress.position();
  }
}

}  // namespace network
