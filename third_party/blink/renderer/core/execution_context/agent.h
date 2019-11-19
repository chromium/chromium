// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8.h"

namespace blink {

namespace scheduler {
class EventLoop;
}

class ExecutionContext;

// Corresponding spec concept is:
// https://html.spec.whatwg.org/C#integration-with-the-javascript-agent-formalism
//
// Agent is a group of browsing contexts that can access each other
// synchronously. E.g. same-site iframes share the same agent, Workers and
// Worklets have their own agent.
// While an WindowAgentFactory is shared across a group of reachable frames,
// Agent is shared across a group of reachable and same-site frames.
class CORE_EXPORT Agent : public GarbageCollected<Agent> {
 public:
  static Agent* CreateForWorkerOrWorklet(
      v8::Isolate* isolate,
      const base::UnguessableToken& cluster_id,
      std::unique_ptr<v8::MicrotaskQueue> microtask_queue = nullptr) {
    return MakeGarbageCollected<Agent>(isolate, cluster_id,
                                       std::move(microtask_queue));
  }

  // Do not create the instance directly.
  // Use Agent::CreateForWorkerOrWorklet() or
  // WindowAgentFactory::GetAgentForOrigin().
  Agent(v8::Isolate* isolate,
        const base::UnguessableToken& cluster_id,
        std::unique_ptr<v8::MicrotaskQueue> microtask_queue = nullptr);
  virtual ~Agent();

  const scoped_refptr<scheduler::EventLoop>& event_loop() const {
    return event_loop_;
  }

  virtual void Trace(blink::Visitor*);

  void AttachExecutionContext(ExecutionContext*);
  void DetachExecutionContext(ExecutionContext*);

  const base::UnguessableToken& cluster_id() const { return cluster_id_; }

 private:
  scoped_refptr<scheduler::EventLoop> event_loop_;
  const base::UnguessableToken cluster_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_
