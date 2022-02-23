// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent.h"


namespace blink {

WindowAgent::WindowAgent(v8::Isolate* isolate)
    : Agent(isolate, base::UnguessableToken::Create()) {}

WindowAgent::WindowAgent(v8::Isolate* isolate,
                         bool is_origin_agent_cluster,
                         bool origin_agent_cluster_left_as_default)
    : Agent(isolate,
            base::UnguessableToken::Create(),
            nullptr,
            is_origin_agent_cluster,
            origin_agent_cluster_left_as_default) {}

WindowAgent::~WindowAgent() = default;

void WindowAgent::Trace(Visitor* visitor) const {
  Agent::Trace(visitor);
}

}  // namespace blink
