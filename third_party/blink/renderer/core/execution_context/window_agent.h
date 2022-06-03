// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_H_

#include "third_party/blink/renderer/core/execution_context/agent.h"

namespace v8 {
class Isolate;
}

namespace blink {

// This corresponds to similar-origin window agent, that is shared by a group
// of Documents that are mutually reachable and have the same-site origins.
// https://html.spec.whatwg.org/C#similar-origin-window-agent
//
// The instance holds per-agent data in addition to the base Agent, that is also
// shared by associated Documents.
class WindowAgent final : public Agent {
 public:
  // Normally you don't want to call this constructor; instead, use
  // WindowAgentFactory::GetAgentForOrigin() so you can get the agent shared
  // on the same-site frames.
  //
  // This constructor creates a unique agent that won't be shared with any
  // other frames. Use this constructor only if:
  //   - An appropriate instance of WindowAgentFactory is not available
  //     (this should only happen in tests).
  explicit WindowAgent(v8::Isolate* isolate);

  ~WindowAgent() override;

  void Trace(Visitor*) const override;

 private:
  // TODO(keishi): Move per-agent data here with the correct granularity.
  // E.g. ActiveMutationObservers and CustomElementReactionStack should move
  // here.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_WINDOW_AGENT_H_
