// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {
namespace scheduler {

perfetto::StaticString FrameOriginTypeToString(FrameOriginType origin) {
  switch (origin) {
    case FrameOriginType::kMainFrame:
      return "main-frame";
    case FrameOriginType::kSameOriginToMainFrame:
      return "same-origin-to-main-frame";
    case FrameOriginType::kCrossOriginToMainFrame:
      return "cross-origin-to-main-frame";
  }
  NOTREACHED();
}

}  // namespace scheduler
}  // namespace blink
