// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {
namespace scheduler {

namespace {

enum class FrameThrottlingState {
  kVisible = 0,
  kVisibleService = 1,
  kHidden = 2,
  kHiddenService = 3,
  kBackground = 4,
  kBackgroundExemptSelf = 5,
  kBackgroundExemptOther = 6,

  kCount = 7
};

enum class FrameOriginState {
  kMainFrame = 0,
  kSameOrigin = 1,
  kCrossOrigin = 2,

  kCount = 3
};

FrameThrottlingState GetFrameThrottlingState(
    const FrameScheduler& frame_scheduler) {
  if (frame_scheduler.IsPageVisible()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisible;
    return FrameThrottlingState::kHidden;
  }

  PageScheduler* page_scheduler = frame_scheduler.GetPageScheduler();
  if (page_scheduler && page_scheduler->IsAudioPlaying()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisibleService;
    return FrameThrottlingState::kHiddenService;
  }

  if (frame_scheduler.IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptSelf;

  if (page_scheduler && page_scheduler->IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptOther;

  return FrameThrottlingState::kBackground;
}

FrameOriginState GetFrameOriginState(const FrameScheduler& frame_scheduler) {
  if (frame_scheduler.GetFrameType() == FrameScheduler::FrameType::kMainFrame) {
    return FrameOriginState::kMainFrame;
  }
  if (frame_scheduler.IsCrossOrigin())
    return FrameOriginState::kCrossOrigin;
  return FrameOriginState::kSameOrigin;
}

}  // namespace

FrameStatus GetFrameStatus(FrameScheduler* frame_scheduler) {
  if (!frame_scheduler)
    return FrameStatus::kNone;
  FrameThrottlingState throttling_state =
      GetFrameThrottlingState(*frame_scheduler);
  FrameOriginState origin_state = GetFrameOriginState(*frame_scheduler);
  return static_cast<FrameStatus>(
      static_cast<int>(FrameStatus::kSpecialCasesCount) +
      static_cast<int>(origin_state) *
          static_cast<int>(FrameThrottlingState::kCount) +
      static_cast<int>(throttling_state));
}

}  // namespace scheduler
}  // namespace blink
