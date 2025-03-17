// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_trace.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"

namespace webnn {

constexpr char kWebNNTraceCategory[] = "webnn";

// Reset the `id_` and `step_name_` fields so the moved `ScopedTrace` object
// won't end the trace prematurely on destruction.
ScopedTrace::ScopedTrace(ScopedTrace&& other)
    : name_(other.name_),
      id_(std::exchange(other.id_, std::nullopt)),
      step_name_(std::exchange(other.step_name_, std::nullopt)) {}

ScopedTrace::~ScopedTrace() {
  if (id_.has_value()) {
    if (step_name_.has_value()) {
      TRACE_EVENT_NESTABLE_ASYNC_END0(kWebNNTraceCategory, *step_name_,
                                      TRACE_ID_LOCAL(id_.value()));
    }
    TRACE_EVENT_NESTABLE_ASYNC_END0(kWebNNTraceCategory, name_,
                                    TRACE_ID_LOCAL(id_.value()));
  }
}

ScopedTrace& ScopedTrace::operator=(ScopedTrace&& other) {
  if (this != &other) {
    name_ = other.name_;
    id_ = std::exchange(other.id_, std::nullopt);
    step_name_ = std::exchange(other.step_name_, std::nullopt);
  }
  return *this;
}

void ScopedTrace::AddStep(const char* step_name) {
  // Calling AddStep() after move is not allowed.
  CHECK(id_.has_value());
  if (step_name_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(kWebNNTraceCategory, *step_name_,
                                    TRACE_ID_LOCAL(id_.value()));
  }
  step_name_ = step_name;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kWebNNTraceCategory, *step_name_,
                                    TRACE_ID_LOCAL(id_.value()));
}

ScopedTrace::ScopedTrace(const char* name)
    : ScopedTrace(name, base::trace_event::GetNextGlobalTraceId()) {}

ScopedTrace::ScopedTrace(const char* name, uint64_t id) : name_(name), id_(id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kWebNNTraceCategory, name_,
                                    TRACE_ID_LOCAL(id_.value()));
}

}  // namespace webnn
