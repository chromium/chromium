// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ScriptToolContext : public GarbageCollected<ScriptToolContext> {
 public:
  explicit ScriptToolContext(const ToolInvocationToken& invocation_id)
      : invocation_id_(invocation_id) {}

  const ToolInvocationToken& GetInvocationId() const { return invocation_id_; }

  void Trace(Visitor*) const {}

 private:
  ToolInvocationToken invocation_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_CONTEXT_H_
