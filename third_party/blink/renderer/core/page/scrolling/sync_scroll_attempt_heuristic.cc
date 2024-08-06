// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/sync_scroll_attempt_heuristic.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

namespace {

SyncScrollAttemptHeuristic* g_sync_scroll_attempt_heuristic = nullptr;

}  // namespace

SyncScrollAttemptHeuristic::SyncScrollAttemptHeuristic(Frame* frame)
    : frame_(frame), last_instance_(g_sync_scroll_attempt_heuristic) {
  if (frame_ && frame_->IsOutermostMainFrame()) {
    g_sync_scroll_attempt_heuristic = this;
  } else {
    g_sync_scroll_attempt_heuristic = nullptr;
  }
}

SyncScrollAttemptHeuristic::~SyncScrollAttemptHeuristic() {
  if (frame_ && frame_->IsOutermostMainFrame()) {
    CHECK_EQ(g_sync_scroll_attempt_heuristic, this);
  }
  g_sync_scroll_attempt_heuristic = last_instance_;
  const bool saw_possible_sync_scrolling_attempt =
      did_access_scroll_offset_ && (did_set_style_ || did_set_scroll_offset_);
  if (saw_possible_sync_scrolling_attempt && frame_ &&
      frame_->IsOutermostMainFrame() && !frame_->IsDetached()) {
    // This will not cover cases where |frame_| is remote.
    if (LocalFrame* local_frame = DynamicTo<LocalFrame>(frame_)) {
      if (local_frame->View()) {
        if (LocalFrameUkmAggregator* ukm_aggregator =
                local_frame->View()->GetUkmAggregator()) {
          ukm_aggregator->RecordCountSample(
              LocalFrameUkmAggregator::kPossibleSynchronizedScrollCount2, 1);
        }
      }
    }
  }
}

SyncScrollAttemptHeuristic::Scope::Scope(bool enable_observation)
    : enable_observation_(enable_observation) {
  if (enable_observation_) {
    SyncScrollAttemptHeuristic::EnableObservation();
  }
}

SyncScrollAttemptHeuristic::Scope::~Scope() {
  if (enable_observation_) {
    SyncScrollAttemptHeuristic::DisableObservation();
  }
}

SyncScrollAttemptHeuristic::Scope
SyncScrollAttemptHeuristic::GetScrollHandlerScope() {
  return Scope(g_sync_scroll_attempt_heuristic);
}

SyncScrollAttemptHeuristic::Scope
SyncScrollAttemptHeuristic::GetRequestAnimationFrameScope() {
  // We only want to observe rAF if one was requested during a scroll
  // handler. If that's the case, |did_request_animation_frame_| should be
  // true.
  return Scope(g_sync_scroll_attempt_heuristic &&
               g_sync_scroll_attempt_heuristic->did_request_animation_frame_);
}

void SyncScrollAttemptHeuristic::DidAccessScrollOffset() {
  if (g_sync_scroll_attempt_heuristic &&
      g_sync_scroll_attempt_heuristic->is_observing_) [[unlikely]] {
    g_sync_scroll_attempt_heuristic->did_access_scroll_offset_ = true;
  }
}

void SyncScrollAttemptHeuristic::DidSetScrollOffset() {
  // We only want to record a mutation if we've already accessed the scroll
  // offset.
  if (g_sync_scroll_attempt_heuristic &&
      g_sync_scroll_attempt_heuristic->is_observing_ &&
      g_sync_scroll_attempt_heuristic->did_access_scroll_offset_) [[unlikely]] {
    g_sync_scroll_attempt_heuristic->did_set_scroll_offset_ = true;
  }
}

void SyncScrollAttemptHeuristic::DidSetStyle() {
  // We only want to record a mutation if we've already accessed the scroll
  // offset.
  if (g_sync_scroll_attempt_heuristic &&
      g_sync_scroll_attempt_heuristic->is_observing_ &&
      g_sync_scroll_attempt_heuristic->did_access_scroll_offset_) [[unlikely]] {
    g_sync_scroll_attempt_heuristic->did_set_style_ = true;
  }
}

void SyncScrollAttemptHeuristic::DidRequestAnimationFrame() {
  if (g_sync_scroll_attempt_heuristic &&
      g_sync_scroll_attempt_heuristic->is_observing_) [[unlikely]] {
    g_sync_scroll_attempt_heuristic->did_request_animation_frame_ = true;
  }
}

void SyncScrollAttemptHeuristic::EnableObservation() {
  if (g_sync_scroll_attempt_heuristic) [[unlikely]] {
    CHECK(!g_sync_scroll_attempt_heuristic->is_observing_);
    g_sync_scroll_attempt_heuristic->is_observing_ = true;
  }
}

void SyncScrollAttemptHeuristic::DisableObservation() {
  if (g_sync_scroll_attempt_heuristic) [[unlikely]] {
    CHECK(g_sync_scroll_attempt_heuristic->is_observing_);
    g_sync_scroll_attempt_heuristic->is_observing_ = false;
  }
}

}  // namespace blink
