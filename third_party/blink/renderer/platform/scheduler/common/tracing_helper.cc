// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"

#include "base/format_macros.h"

namespace blink {
namespace scheduler {

constexpr const char TracingCategoryName::kTopLevel[];
constexpr const char TracingCategoryName::kDefault[];
constexpr const char TracingCategoryName::kInfo[];
constexpr const char TracingCategoryName::kDebug[];

namespace internal {

void ValidateTracingCategory(const char* category) {
  // Category must be a constant defined in tracing helper because there's no
  // portable way to use string literals as a template argument.
  // Unfortunately, static_assert won't work with templates either because
  // inequality (!=) of linker symbols is undefined in compile-time.
  DCHECK(category == TracingCategoryName::kTopLevel ||
         category == TracingCategoryName::kDefault ||
         category == TracingCategoryName::kInfo ||
         category == TracingCategoryName::kDebug);
}

}  // namespace internal

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
