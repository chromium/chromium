// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {
namespace scheduler {

FrameOriginType GetFrameOriginType(FrameScheduler* scheduler) {
  DCHECK(scheduler);

  if (scheduler->GetFrameType() == FrameScheduler::FrameType::kMainFrame)
    return FrameOriginType::kMainFrame;

  if (scheduler->IsCrossOriginToNearestMainFrame()) {
    return FrameOriginType::kCrossOriginToMainFrame;
  } else {
    return FrameOriginType::kSameOriginToMainFrame;
  }
}

const char* FrameOriginTypeToString(FrameOriginType origin) {
  switch (origin) {
    case FrameOriginType::kMainFrame:
      return "main-frame";
    case FrameOriginType::kSameOriginToMainFrame:
      return "same-origin-to-main-frame";
    case FrameOriginType::kCrossOriginToMainFrame:
      return "cross-origin-to-main-frame";
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
