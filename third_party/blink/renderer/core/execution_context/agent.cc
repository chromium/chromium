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
bool is_isolated_context = false;
bool is_web_security_disabled = false;

#if DCHECK_IS_ON()
bool is_isolated_context_set = false;
bool is_web_security_disabled_set = false;
#endif
}  // namespace

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             AgentType agent_type,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue)
    : Agent(isolate,
            cluster_id,
            std::move(microtask_queue),
            AgentClusterKey::CreateSiteKeyed(KURL()),
            agent_type) {}

Agent::Agent(v8::Isolate* isolate,
             const base::UnguessableToken& cluster_id,
             std::unique_ptr<v8::MicrotaskQueue> microtask_queue,
             const AgentClusterKey& agent_cluster_key,
             AgentType agent_type)
    : isolate_(isolate),
      rejected_promises_(RejectedPromises::Create()),
      event_loop_(base::AdoptRef(
          new scheduler::EventLoop(this, isolate, std::move(microtask_queue)))),
      cluster_id_(cluster_id),
      agent_cluster_key_(agent_cluster_key),
      agent_type_(agent_type) {}

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

bool Agent::IsCrossOriginIsolated() const {
  switch (agent_type_) {
    case AgentType::kDocument:
      return agent_cluster_key_.GetCrossOriginIsolationKey() &&
             agent_cluster_key_.GetCrossOriginIsolationKey()->mode ==
                 mojom::blink::CrossOriginIsolationMode::kConcrete;
    case AgentType::kNonCrossOriginIsolatedWorker:
      return false;
    case AgentType::kCrossOriginIsolatedWorker:
      return true;
    default:
  }
  NOTREACHED();
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
