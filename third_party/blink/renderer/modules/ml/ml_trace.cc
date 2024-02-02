// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_trace.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"

namespace blink {

constexpr char kWebNNTraceCategory[] = "webnn";

// Reset the |id_| so the moved `ScopedMLTrace` object won't end the trace
// prematurely on destruction.
ScopedMLTrace::ScopedMLTrace(ScopedMLTrace&& other)
    : name_(other.name_),
      id_(std::exchange(other.id_, std::nullopt)),
      step_(std::move(other.step_)) {}

ScopedMLTrace::~ScopedMLTrace() {
  if (id_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(kWebNNTraceCategory, name_,
                                    TRACE_ID_LOCAL(id_.value()));
  }
}

ScopedMLTrace& ScopedMLTrace::operator=(ScopedMLTrace&& other) {
  if (this != &other) {
    name_ = other.name_;
    id_ = std::exchange(other.id_, std::nullopt);
    step_ = std::move(other.step_);
  }
  return *this;
}

void ScopedMLTrace::AddStep(const char* step_name) {
  // Calling AddStep() after move is not allowed.
  CHECK(id_.has_value());
  step_.reset();
  step_ = base::WrapUnique(new ScopedMLTrace(step_name, id_.value()));
}

ScopedMLTrace::ScopedMLTrace(const char* name)
    : ScopedMLTrace(name, base::trace_event::GetNextGlobalTraceId()) {}

ScopedMLTrace::ScopedMLTrace(const char* name, uint64_t id)
    : name_(name), id_(id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kWebNNTraceCategory, name_,
                                    TRACE_ID_LOCAL(id_.value()));
}

}  // namespace blink
