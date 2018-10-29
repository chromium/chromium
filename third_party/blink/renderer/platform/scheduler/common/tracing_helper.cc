// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace blink {
namespace scheduler {

const char kTracingCategoryNameTopLevel[] = "toplevel";
const char kTracingCategoryNameDefault[] = "renderer.scheduler";
const char kTracingCategoryNameInfo[] =
    TRACE_DISABLED_BY_DEFAULT("renderer.scheduler");
const char kTracingCategoryNameDebug[] =
    TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug");

namespace internal {

void ValidateTracingCategory(const char* category) {
  // Category must be a constant defined in tracing helper because there's no
  // portable way to use string literals as a template argument.
  // Unfortunately, static_assert won't work with templates either because
  // inequality (!=) of linker symbols is undefined in compile-time.
  DCHECK(category == kTracingCategoryNameTopLevel ||
         category == kTracingCategoryNameDefault ||
         category == kTracingCategoryNameInfo ||
         category == kTracingCategoryNameDebug);
}

}  // namespace internal

void WarmupTracingCategories() {
  // No need to warm-up toplevel category here.
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameDefault);
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameInfo);
  TRACE_EVENT_WARMUP_CATEGORY(kTracingCategoryNameDebug);
}

std::string PointerToString(const void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

double TimeDeltaToMilliseconds(const base::TimeDelta& value) {
  return value.InMillisecondsF();
}

const char* YesNoStateToString(bool is_yes) {
  if (is_yes) {
    return "yes";
  } else {
    return "no";
  }
}

TraceableVariableController::TraceableVariableController() {}

TraceableVariableController::~TraceableVariableController() {
  // Controller should have very same lifetime as their tracers.
  DCHECK(traceable_variables_.empty());
}

void TraceableVariableController::RegisterTraceableVariable(
    TraceableVariable* traceable_variable) {
  traceable_variables_.insert(traceable_variable);
}

void TraceableVariableController::DeregisterTraceableVariable(
    TraceableVariable* traceable_variable) {
  traceable_variables_.erase(traceable_variable);
}

void TraceableVariableController::OnTraceLogEnabled() {
  for (auto* tracer : traceable_variables_) {
    tracer->OnTraceLogEnabled();
  }
}

}  // namespace scheduler
}  // namespace blink
