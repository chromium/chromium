// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/agent_cluster_key.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-microtask-queue.h"

namespace blink {

class ExecutionContext;
class RejectedPromises;

// Corresponding spec concept is:
// https://html.spec.whatwg.org/C#integration-with-the-javascript-agent-formalism
//
// Agent is a group of browsing contexts that can access each other
// synchronously. E.g. same-site iframes share the same agent, Workers and
// Worklets have their own agent.
// While an WindowAgentFactory is shared across a group of reachable frames,
// Agent is shared across a group of reachable and same-site frames.
class CORE_EXPORT Agent : public GarbageCollected<Agent>,
                          public Supplementable<Agent>,
                          public scheduler::EventLoop::Delegate {
 public:
  // The type of contexts hosted in the Agent.
  enum class AgentType {
    kDocument,
    kNonCrossOriginIsolatedWorker,
    kCrossOriginIsolatedWorker
  };

  // Do not create the instance directly.
  // Use MakeGarbageCollected<Agent>() or
  // WindowAgentFactory::GetAgentForAgentClusterKey().
  Agent(v8::Isolate* isolate,
        const base::UnguessableToken& cluster_id,
        AgentType agent_type,
        std::unique_ptr<v8::MicrotaskQueue> microtask_queue = nullptr);
  virtual ~Agent();

  const scoped_refptr<scheduler::EventLoop>& event_loop() const {
    return event_loop_;
  }

  v8::Isolate* isolate() { return isolate_; }

  void Trace(Visitor*) const override;

  void AttachContext(ExecutionContext*);
  void DetachContext(ExecutionContext*);

  const base::UnguessableToken& cluster_id() const { return cluster_id_; }

  // Representing agent cluster's "cross-origin isolated" concept.
  // https://html.spec.whatwg.org/multipage/webappapis.html#agent-cluster-cross-origin-isolation
  bool IsCrossOriginIsolated() const;

  static bool IsWebSecurityDisabled();
  static void SetIsWebSecurityDisabled(bool value);

  // Represents adherence to an additional set of restrictions above and beyond
  // "cross-origin isolated".
  //
  // TODO(mkwst): We need a specification for these restrictions:
  // https://crbug.com/1206150.
  static bool IsIsolatedContext();
  static void ResetIsIsolatedContextForTest();
  // Only called from blink::SetIsIsolatedContext.
  static void SetIsIsolatedContext(bool value);

  // The AgentClusterKey represents the set of contexts that can be hosted by
  // this agent.
  // https://html.spec.whatwg.org/multipage/webappapis.html#agent-cluster-key
  const AgentClusterKey& GetAgentClusterKey() const {
    return agent_cluster_key_;
  }

  // Returns if this is a Window Agent or not.
  virtual bool IsWindowAgent() const;

  virtual void Dispose();
  virtual void PerformMicrotaskCheckpoint();

  RejectedPromises& GetRejectedPromises();

 protected:
  Agent(v8::Isolate* isolate,
        const base::UnguessableToken& cluster_id,
        std::unique_ptr<v8::MicrotaskQueue> microtask_queue,
        const AgentClusterKey& agent_cluster_key,
        AgentType agent_type);

 private:
  // scheduler::EventLoopDelegate overrides:
  void NotifyRejectedPromises() override;

  v8::Isolate* isolate_;
  scoped_refptr<RejectedPromises> rejected_promises_;
  scoped_refptr<scheduler::EventLoop> event_loop_;
  const base::UnguessableToken cluster_id_;
  const AgentClusterKey agent_cluster_key_;
  const AgentType agent_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_
