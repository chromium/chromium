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
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

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
  explicit ProgressItem(long long length)
      : bytes_received(0), estimated_length(length) {}

  long long bytes_received;
  long long estimated_length;

  DISALLOW_COPY_AND_ASSIGN(ProgressItem);
};

ProgressTracker* ProgressTracker::Create(LocalFrame* frame) {
  return new ProgressTracker(frame);
}

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
    probe::frameStartedLoading(frame_);
  }
}

void ProgressTracker::ProgressCompleted() {
  DCHECK(frame_->IsLoading());
  frame_->SetIsLoading(false);
  SendFinalProgress();
  Reset();
  GetLocalFrameClient()->DidStopLoading();
  probe::frameStoppedLoading(frame_);
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

void ProgressTracker::WillStartLoading(unsigned long identifier,
                                       ResourceLoadPriority priority) {
  if (!frame_->IsLoading())
    return;
  if (HaveParsedAndPainted() || priority < ResourceLoadPriority::kHigh)
    return;
  progress_items_.Set(identifier, std::make_unique<ProgressItem>(
                                      kProgressItemDefaultEstimatedLength));
}

void ProgressTracker::IncrementProgress(unsigned long identifier,
                                        const ResourceResponse& response) {
  ProgressItem* item = progress_items_.at(identifier);
  if (!item)
    return;

  long long estimated_length = response.ExpectedContentLength();
  if (estimated_length < 0)
    estimated_length = kProgressItemDefaultEstimatedLength;
  item->bytes_received = 0;
  item->estimated_length = estimated_length;
}

void ProgressTracker::IncrementProgress(unsigned long identifier, int length) {
  ProgressItem* item = progress_items_.at(identifier);
  if (!item)
    return;

  item->bytes_received += length;
  if (item->bytes_received > item->estimated_length)
    item->estimated_length = item->bytes_received * 2;
  MaybeSendProgress();
}

bool ProgressTracker::HaveParsedAndPainted() {
  return finished_parsing_ && did_first_contentful_paint_;
}

void ProgressTracker::MaybeSendProgress() {
  if (!frame_->IsLoading())
    return;

  progress_value_ = kInitialProgressValue + 0.1;  // +0.1 for committing
  if (finished_parsing_)
    progress_value_ += 0.1;
  if (did_first_contentful_paint_)
    progress_value_ += 0.1;

  long long bytes_received = 0;
  long long estimated_bytes_for_pending_requests = 0;
  for (const auto& progress_item : progress_items_) {
    bytes_received += progress_item.value->bytes_received;
    estimated_bytes_for_pending_requests +=
        progress_item.value->estimated_length;
  }
  DCHECK_GE(estimated_bytes_for_pending_requests, 0);
  DCHECK_GE(estimated_bytes_for_pending_requests, bytes_received);

  if (HaveParsedAndPainted() &&
      estimated_bytes_for_pending_requests == bytes_received) {
    SendFinalProgress();
    return;
  }

  double percent_of_bytes_received =
      !estimated_bytes_for_pending_requests
          ? 1.0
          : (double)bytes_received /
                (double)estimated_bytes_for_pending_requests;
  progress_value_ += percent_of_bytes_received / 2;

  DCHECK_GE(progress_value_, kInitialProgressValue);
  // Always leave space at the end. This helps show the user that we're not
  // done until we're done.
  DCHECK_LE(progress_value_, 0.9);
  if (progress_value_ < last_notified_progress_value_)
    return;

  double now = CurrentTime();
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

void ProgressTracker::CompleteProgress(unsigned long identifier) {
  ProgressItem* item = progress_items_.at(identifier);
  if (!item)
    return;

  item->estimated_length = item->bytes_received;
  MaybeSendProgress();
}

}  // namespace blink
