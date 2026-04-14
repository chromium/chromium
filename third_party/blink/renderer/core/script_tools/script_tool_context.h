// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ScriptToolContext : public GarbageCollected<ScriptToolContext> {
 public:
  explicit ScriptToolContext(const base::UnguessableToken& invocation_id)
      : invocation_id_(invocation_id) {}

  const base::UnguessableToken& GetInvocationId() const {
    return invocation_id_;
  }

  void Trace(Visitor*) const {}

 private:
  base::UnguessableToken invocation_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_
