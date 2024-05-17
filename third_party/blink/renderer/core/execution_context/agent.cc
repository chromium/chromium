// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent.h"

#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

namespace {
bool is_cross_origin_isolated = false;
bool is_isolated_context = false;
bool is_web_security_disabled = false;

#if DCHECK_IS_ON()
bool is_cross_origin_isolated_set = false;
bool is_isolated_context_set = false;
bool is_web_security_disabled_set = false;
#endif
}  // namespace

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : Agent(isolate, cluster_id, std::move(microtask_queue), false, true) {}

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue,
             bool is_origin_agent_cluster,
             bool origin_agent_cluster_left_as_default)
    : isolate_(isolate),
      rejected_promises_(RejectedPromises::Create()),
      event_loop_(base::AdoptRef(
          new scheduler::EventLoop(this, isolate, std::move(microtask_queue)))),
      cluster_id_(cluster_id),
      origin_keyed_because_of_inheritance_(false),
      is_origin_agent_cluster_(is_origin_agent_cluster),
      origin_agent_cluster_left_as_default_(
          origin_agent_cluster_left_as_default) {}

Agent::~Agent() = default;

void Agent::Trace(Visitor* visitor) const {
  Supplementable<Agent>::Trace(visitor);
}

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

// static
bool Agent::IsWebSecurityDisabled() {
  return is_web_security_disabled;
}

// static
void Agent::SetIsWebSecurityDisabled(bool value) {
#if DCHECK_IS_ON()
  if (is_web_security_disabled_set) {
    DCHECK_EQ(is_web_security_disabled, value);
  }
  is_web_security_disabled_set = true;
#endif
  is_web_security_disabled = value;
}

// static
bool Agent::IsIsolatedContext() {
  return is_isolated_context;
}

// static
void Agent::ResetIsIsolatedContextForTest() {
#if DCHECK_IS_ON()
  is_isolated_context_set = false;
#endif
  is_isolated_context = false;
}

// static
void Agent::SetIsIsolatedContext(bool value) {
#if DCHECK_IS_ON()
  if (is_isolated_context_set)
    DCHECK_EQ(is_isolated_context, value);
  is_isolated_context_set = true;
#endif
  is_isolated_context = value;
}

bool Agent::IsOriginKeyed() const {
  return IsCrossOriginIsolated() || IsOriginKeyedForInheritance();
}

bool Agent::IsOriginKeyedForInheritance() const {
  return is_origin_agent_cluster_ || origin_keyed_because_of_inheritance_;
}

bool Agent::IsOriginOrSiteKeyedBasedOnDefault() const {
  return origin_agent_cluster_left_as_default_;
}

void Agent::ForceOriginKeyedBecauseOfInheritance() {
  origin_keyed_because_of_inheritance_ = true;
}

bool Agent::IsWindowAgent() const {
  return false;
}

void Agent::PerformMicrotaskCheckpoint() {
  event_loop_->PerformMicrotaskCheckpoint();
}

void Agent::Dispose() {
  rejected_promises_->Dispose();
}

RejectedPromises& Agent::GetRejectedPromises() {
  return *rejected_promises_;
}

void Agent::NotifyRejectedPromises() {
  rejected_promises_->ProcessQueue();
}

}  // namespace blink
