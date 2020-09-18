// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {
bool is_cross_origin_isolated = false;

#if DCHECK_IS_ON()
bool is_cross_origin_isolated_set = false;
#endif
}  // namespace

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : event_loop_(base::AdoptRef(
          new scheduler::EventLoop(isolate, std::move(microtask_queue)))),
      cluster_id_(cluster_id) {}

Agent::~Agent() = default;

void Agent::Trace(Visitor* visitor) const {}

void Agent::AttachContext(ExecutionContext* context) {
  event_loop_->AttachScheduler(context->GetScheduler());
}

void Agent::DetachContext(ExecutionContext* context) {
  event_loop_->DetachScheduler(context->GetScheduler());
}

// static
bool Agent::IsCrossOriginIsolated() {
  return is_cross_origin_isolated;
}

// static
void Agent::SetIsCrossOriginIsolated(bool value) {
#if DCHECK_IS_ON()
  if (is_cross_origin_isolated_set)
    DCHECK_EQ(is_cross_origin_isolated, value);
  is_cross_origin_isolated_set = true;
#endif
  is_cross_origin_isolated = value;
}

bool Agent::IsOriginIsolated() {
#if DCHECK_IS_ON()
  DCHECK(is_origin_isolated_set_);
#endif
  return is_origin_isolated_;
}

void Agent::SetIsOriginIsolated(bool value) {
#if DCHECK_IS_ON()
  DCHECK(!is_origin_isolated_set_ || value == is_origin_isolated_);
  is_origin_isolated_set_ = true;
#endif
  is_origin_isolated_ = value;
}

}  // namespace blink
