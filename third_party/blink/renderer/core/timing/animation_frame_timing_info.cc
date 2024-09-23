// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"

#include "base/trace_event/trace_id_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

ScriptTimingInfo::ScriptTimingInfo(ExecutionContext* context,
                                   InvokerType type,
                                   base::TimeTicks start_time,
                                   base::TimeTicks execution_start_time,
                                   base::TimeTicks end_time,
                                   base::TimeDelta style_duration,
                                   base::TimeDelta layout_duration)
    : invoker_type_(type),
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

uint64_t AnimationFrameTimingInfo::GetTraceId() const {
  // Lazily initialize trace id since it's only used if tracing is enabled.
  if (trace_id_ != 0) {
    return trace_id_;
  }
  trace_id_ = base::trace_event::GetNextGlobalTraceId();
  return trace_id_;
}
}  // namespace blink
