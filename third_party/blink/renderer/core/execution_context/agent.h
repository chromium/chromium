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
  // Do not create the instance directly.
  // Use MakeGarbageCollected<Agent>() or
  // WindowAgentFactory::GetAgentForOrigin().
  Agent(v8::Isolate* isolate,
        const base::UnguessableToken& cluster_id,
        std::unique_ptr<v8::MicrotaskQueue> microtask_queue = nullptr);
  virtual ~Agent();

  const scoped_refptr<scheduler::EventLoop>& event_loop() const {
    return event_loop_;
  }

  virtual void Trace(Visitor*) const;

  void AttachContext(ExecutionContext*);
  void DetachContext(ExecutionContext*);

  const base::UnguessableToken& cluster_id() const { return cluster_id_; }

  // Representing agent cluster's "cross-origin isolated" concept.
  // TODO(yhirano): Have the spec URL.
  // This property is renderer process global because we ensure that a
  // renderer process host only cross-origin isolated agents or only
  // non-cross-origin isolated agents, not both.
  // This variable is initialized before any frame is created, and will not
  // be modified after that. Hence this can be accessed from the main thread
  // and worker/worklet threads.
  static bool IsCrossOriginIsolated();
  // Only called from blink::SetIsCrossOriginIsolated.
  static void SetIsCrossOriginIsolated(bool value);

  // Representing agent cluster's "origin isolated" concept.
  // https://github.com/whatwg/html/pull/5545
  // TODO(domenic): update to final spec URL when that pull request is merged.
  //
  // Note that unlike IsCrossOriginIsolated(), this is not static/process-global
  // because we do not guarantee that a given process only contains agents with
  // the same origin-isolation status.
  //
  // For example, a page with no Origin-Isolation header, that uses a data: URL
  // to create an iframe, would have an origin-isolated data: URL Agent, plus a
  // non-origin-isolated outer page Agent, both in the same process.
  bool IsOriginIsolated();
  void SetIsOriginIsolated(bool value);

 private:
  scoped_refptr<scheduler::EventLoop> event_loop_;
  const base::UnguessableToken cluster_id_;
  bool is_origin_isolated_ = false;

#if DCHECK_IS_ON()
  bool is_origin_isolated_set_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_H_
