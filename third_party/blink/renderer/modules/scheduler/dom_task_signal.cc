// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

DOMTaskSignal::DOMTaskSignal(ExecutionContext* context,
                             const AtomicString& priority)
    : AbortSignal(context), priority_(priority) {}

DOMTaskSignal::~DOMTaskSignal() = default;

AtomicString DOMTaskSignal::priority() {
  return priority_;
}

void DOMTaskSignal::AddPriorityChangeAlgorithm(base::OnceClosure algorithm) {
  priority_change_algorithms_.push_back(std::move(algorithm));
}

void DOMTaskSignal::SignalPriorityChange(const AtomicString& priority) {
  if (priority_ == priority)
    return;
  priority_ = priority;

  for (base::OnceClosure& closure : priority_change_algorithms_) {
    std::move(closure).Run();
  }
  priority_change_algorithms_.clear();

  priority_change_status_ = PriorityChangeStatus::kPriorityHasChanged;
  DispatchEvent(*Event::Create(event_type_names::kPrioritychange));
}

void DOMTaskSignal::Trace(Visitor* visitor) const {
  AbortSignal::Trace(visitor);
}

}  // namespace blink
