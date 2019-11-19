/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/progress_tracker.h"

#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// Always start progress at initialProgressValue. This helps provide feedback as
// soon as a load starts.
static const double kInitialProgressValue = 0.1;

static const int kProgressItemDefaultEstimatedLength = 1024 * 1024;

static const double kProgressNotificationInterval = 0.02;
static const double kProgressNotificationTimeInterval = 0.1;

struct ProgressItem {
  USING_FAST_MALLOC(ProgressItem);
 public:
  int64_t bytes_received = 0;
  int64_t estimated_length = 0;
};

ProgressTracker::ProgressTracker(LocalFrame* frame)
    : frame_(frame),
      last_notified_progress_value_(0),
      last_notified_progress_time_(0),
      finished_parsing_(false),
      did_first_contentful_paint_(false),
      progress_value_(0) {}

ProgressTracker::~ProgressTracker() = default;

void ProgressTracker::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
}

void ProgressTracker::Dispose() {
  if (frame_->IsLoading())
    ProgressCompleted();
  DCHECK(!frame_->IsLoading());
}

double ProgressTracker::EstimatedProgress() const {
  return progress_value_;
}

void ProgressTracker::Reset() {
  progress_items_.clear();
  progress_value_ = 0;
  last_notified_progress_value_ = 0;
  last_notified_progress_time_ = 0;
  finished_parsing_ = false;
  did_first_contentful_paint_ = false;
  bytes_received_ = 0;
  estimated_bytes_for_pending_requests_ = 0;
}

LocalFrameClient* ProgressTracker::GetLocalFrameClient() const {
  return frame_->Client();
}

void ProgressTracker::ProgressStarted() {
  Reset();
  progress_value_ = kInitialProgressValue;
  if (!frame_->IsLoading()) {
    GetLocalFrameClient()->DidStartLoading();
    frame_->SetIsLoading(true);
    probe::FrameStartedLoading(frame_);
  }
}

void ProgressTracker::ProgressCompleted() {
  DCHECK(frame_->IsLoading());
  frame_->SetIsLoading(false);
  SendFinalProgress();
  Reset();
  GetLocalFrameClient()->DidStopLoading();
  probe::FrameStoppedLoading(frame_);
}

void ProgressTracker::FinishedParsing() {
  finished_parsing_ = true;
  MaybeSendProgress();
}

void ProgressTracker::DidFirstContentfulPaint() {
  did_first_contentful_paint_ = true;
  MaybeSendProgress();
}

void ProgressTracker::SendFinalProgress() {
  if (progress_value_ == 1)
    return;
  progress_value_ = 1;
  GetLocalFrameClient()->ProgressEstimateChanged(progress_value_);
}

void ProgressTracker::WillStartLoading(uint64_t identifier,
                                       ResourceLoadPriority priority) {
  if (!frame_->IsLoading())
    return;
  if (HaveParsedAndPainted() || priority < ResourceLoadPriority::kHigh)
    return;
  ProgressItem new_item;
  UpdateProgressItem(new_item, 0, kProgressItemDefaultEstimatedLength);
  progress_items_.Set(identifier, new_item);
}

void ProgressTracker::IncrementProgress(uint64_t identifier,
                                        const ResourceResponse& response) {
  auto item = progress_items_.find(identifier);
  if (item == progress_items_.end())
    return;

  int64_t estimated_length = response.ExpectedContentLength();
  if (estimated_length < 0)
    estimated_length = kProgressItemDefaultEstimatedLength;
  UpdateProgressItem(item->value, 0, estimated_length);
}

void ProgressTracker::IncrementProgress(uint64_t identifier, uint64_t length) {
  auto item = progress_items_.find(identifier);
  if (item == progress_items_.end())
    return;

  ProgressItem& progress_item = item->value;
  int64_t bytes_received = progress_item.bytes_received + length;
  int64_t estimated_length = bytes_received > progress_item.estimated_length
                                 ? bytes_received * 2
                                 : progress_item.estimated_length;
  UpdateProgressItem(progress_item, bytes_received, estimated_length);
  MaybeSendProgress();
}

bool ProgressTracker::HaveParsedAndPainted() {
  return finished_parsing_ && did_first_contentful_paint_;
}

void ProgressTracker::UpdateProgressItem(ProgressItem& item,
                                         int64_t bytes_received,
                                         int64_t estimated_length) {
  bytes_received_ += (bytes_received - item.bytes_received);
  estimated_bytes_for_pending_requests_ +=
      (estimated_length - item.estimated_length);
  DCHECK_GE(bytes_received_, 0);
  DCHECK_GE(estimated_bytes_for_pending_requests_, bytes_received_);

  item.bytes_received = bytes_received;
  item.estimated_length = estimated_length;
}

void ProgressTracker::MaybeSendProgress() {
  if (!frame_->IsLoading())
    return;

  progress_value_ = kInitialProgressValue + 0.1;  // +0.1 for committing
  if (finished_parsing_)
    progress_value_ += 0.1;
  if (did_first_contentful_paint_)
    progress_value_ += 0.1;

  if (HaveParsedAndPainted() &&
      estimated_bytes_for_pending_requests_ == bytes_received_) {
    SendFinalProgress();
    return;
  }

  double percent_of_bytes_received =
      !estimated_bytes_for_pending_requests_
          ? 1.0
          : (double)bytes_received_ /
                (double)estimated_bytes_for_pending_requests_;
  progress_value_ += percent_of_bytes_received / 2;

  DCHECK_GE(progress_value_, kInitialProgressValue);
  // Always leave space at the end. This helps show the user that we're not
  // done until we're done.
  DCHECK_LE(progress_value_, 0.9);
  if (progress_value_ < last_notified_progress_value_)
    return;

  double now = base::Time::Now().ToDoubleT();
  double notified_progress_time_delta = now - last_notified_progress_time_;

  double notification_progress_delta =
      progress_value_ - last_notified_progress_value_;
  if (notification_progress_delta >= kProgressNotificationInterval ||
      notified_progress_time_delta >= kProgressNotificationTimeInterval) {
    GetLocalFrameClient()->ProgressEstimateChanged(progress_value_);
    last_notified_progress_value_ = progress_value_;
    last_notified_progress_time_ = now;
  }
}

void ProgressTracker::CompleteProgress(uint64_t identifier) {
  auto item = progress_items_.find(identifier);
  if (item == progress_items_.end())
    return;

  ProgressItem& progress_item = item->value;
  UpdateProgressItem(item->value, progress_item.bytes_received,
                     progress_item.bytes_received);
  MaybeSendProgress();
}

}  // namespace blink
