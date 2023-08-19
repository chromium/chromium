// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

ScriptTimingInfo::ScriptTimingInfo(ExecutionContext* context,
                                   Type type,
                                   base::TimeTicks start_time,
                                   base::TimeTicks execution_start_time,
                                   base::TimeTicks end_time,
                                   base::TimeDelta style_duration,
                                   base::TimeDelta layout_duration)
    : type_(type),
      start_time_(start_time),
      execution_start_time_(execution_start_time),
      end_time_(end_time),
      style_duration_(style_duration),
      layout_duration_(layout_duration),
      window_(DynamicTo<LocalDOMWindow>(context)),
      security_origin_(context->GetSecurityOrigin()) {
  CHECK(security_origin_);
}

void ScriptTimingInfo::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
}

void AnimationFrameTimingInfo::Trace(Visitor* visitor) const {
  visitor->Trace(scripts_);
}
}  // namespace blink
