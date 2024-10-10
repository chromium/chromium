// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_script_timing.h"

#include <cstdint>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_invoker_type.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PerformanceScriptTiming::PerformanceScriptTiming(
    ScriptTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    DOMWindow* source)
    : PerformanceEntry(
          (info->EndTime() - info->StartTime()).InMilliseconds(),
          performance_entry_names::kScript,
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(info->StartTime()),
          source) {
  info_ = info;
  time_origin_ = time_origin;
  cross_origin_isolated_capability_ = cross_origin_isolated_capability;
  if (!info_->Window() || !source) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kOther;
  } else if (info_->Window() == source) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kSelf;
  } else if (!info_->Window()->GetFrame()) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kOther;
  } else if (info_->Window()->GetFrame()->Tree().IsDescendantOf(
                 source->GetFrame())) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kDescendant;
  } else if (source->GetFrame()->Tree().IsDescendantOf(
                 info_->Window()->GetFrame())) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kAncestor;
  } else if (source->GetFrame()->Tree().Top() ==
             info_->Window()->GetFrame()->Top()) {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kSamePage;
  } else {
    window_attribution_ = V8ScriptWindowAttribution::Enum::kOther;
  }
}

PerformanceScriptTiming::~PerformanceScriptTiming() = default;

const AtomicString& PerformanceScriptTiming::entryType() const {
  return performance_entry_names::kScript;
}

AtomicString PerformanceScriptTiming::invoker() const {
  switch (info_->GetInvokerType()) {
    case ScriptTimingInfo::InvokerType::kClassicScript:
    case ScriptTimingInfo::InvokerType::kModuleScript: {
      if (info_->GetSourceLocation().url) {
        return AtomicString(info_->GetSourceLocation().url);
      }
      if (const DOMWindow* owner_window = source()) {
        CHECK(owner_window->IsLocalDOMWindow());
        return AtomicString(
            To<LocalDOMWindow>(owner_window)->BaseURL().GetString());
      }
      return AtomicString("inline");
    }
    case ScriptTimingInfo::InvokerType::kEventHandler:
    case ScriptTimingInfo::InvokerType::kUserCallback: {
      WTF::StringBuilder builder;
      if (info_->GetInvokerType() ==
          ScriptTimingInfo::InvokerType::kEventHandler) {
        builder.Append(info_->ClassLikeName());
        builder.Append(".");
        builder.Append("on");
      }
      builder.Append(info_->PropertyLikeName());
      return builder.ToAtomicString();
    }

    case ScriptTimingInfo::InvokerType::kPromiseResolve:
    case ScriptTimingInfo::InvokerType::kPromiseReject: {
      WTF::StringBuilder builder;
      if (info_->PropertyLikeName().empty()) {
        return AtomicString(
            info_->GetInvokerType() ==
                    ScriptTimingInfo::InvokerType::kPromiseResolve
                ? "Promise.resolve"
                : "Promise.reject");
      }

      if (!info_->ClassLikeName().empty()) {
        builder.Append(info_->ClassLikeName());
        builder.Append(".");
      }
      builder.Append(info_->PropertyLikeName());
      builder.Append(".");
      builder.Append(info_->GetInvokerType() ==
                             ScriptTimingInfo::InvokerType::kPromiseResolve
                         ? "then"
                         : "catch");
      return builder.ToAtomicString();
    }
  }
}
DOMHighResTimeStamp PerformanceScriptTiming::executionStart() const {
  return ToMonotonicTime(info_->ExecutionStartTime());
}

DOMHighResTimeStamp PerformanceScriptTiming::ToMonotonicTime(
    base::TimeTicks time) const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, time, /*allow_negative_value=*/false,
      cross_origin_isolated_capability_);
}

DOMHighResTimeStamp PerformanceScriptTiming::forcedStyleAndLayoutDuration()
    const {
  return (info_->StyleDuration() + info_->LayoutDuration()).InMilliseconds();
}

DOMHighResTimeStamp PerformanceScriptTiming::pauseDuration() const {
  return info_->PauseDuration().InMilliseconds();
}

LocalDOMWindow* PerformanceScriptTiming::window() const {
  return info_->Window();
}

V8ScriptWindowAttribution PerformanceScriptTiming::windowAttribution() const {
  return V8ScriptWindowAttribution(window_attribution_);
}

V8ScriptInvokerType PerformanceScriptTiming::invokerType() const {
  switch (info_->GetInvokerType()) {
    case ScriptTimingInfo::InvokerType::kClassicScript:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kClassicScript);
    case ScriptTimingInfo::InvokerType::kModuleScript:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kModuleScript);
    case ScriptTimingInfo::InvokerType::kEventHandler:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kEventListener);
    case ScriptTimingInfo::InvokerType::kUserCallback:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kUserCallback);
    case ScriptTimingInfo::InvokerType::kPromiseResolve:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kResolvePromise);
    case ScriptTimingInfo::InvokerType::kPromiseReject:
      return V8ScriptInvokerType(V8ScriptInvokerType::Enum::kRejectPromise);
  }
  NOTREACHED();
}

WTF::String PerformanceScriptTiming::sourceURL() const {
  return info_->GetSourceLocation().url;
}
WTF::String PerformanceScriptTiming::sourceFunctionName() const {
  return info_->GetSourceLocation().function_name;
}
int32_t PerformanceScriptTiming::sourceCharPosition() const {
  return info_->GetSourceLocation().char_position;
}

PerformanceEntryType PerformanceScriptTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kScript;
}

void PerformanceScriptTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("invoker", invoker());
  builder.AddString("invokerType", invokerType().AsString());
  builder.AddString("windowAttribution", windowAttribution().AsString());
  builder.AddNumber("executionStart", executionStart());
  builder.AddNumber("forcedStyleAndLayoutDuration",
                    forcedStyleAndLayoutDuration());
  builder.AddNumber("pauseDuration", pauseDuration());
  builder.AddString("sourceURL", sourceURL());
  builder.AddString("sourceFunctionName", sourceFunctionName());
  builder.AddNumber("sourceCharPosition", sourceCharPosition());
}

void PerformanceScriptTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(info_);
}

}  // namespace blink
