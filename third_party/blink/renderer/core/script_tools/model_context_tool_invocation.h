// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TOOL_INVOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TOOL_INVOCATION_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ModelContext;
class ScriptState;

class CORE_EXPORT ModelContextToolInvocation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ModelContextToolInvocation(ModelContext* model_context,
                             const ToolInvocationToken& invocation_id);
  ~ModelContextToolInvocation() override = default;

  ScriptPromise<IDLString> requestToken(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

 private:
  // There is an N:1 relationship between `ModelContextToolInvocation` and
  // `ModelContext` (one invocation instance per tool step executed on a
  // `ModelContext`). Because `this` holds a strong `Member` to `ModelContext`,
  // the lifetime of `this` is a strict subset of `ModelContext`'s lifetime.
  Member<ModelContext> model_context_;
  const ToolInvocationToken invocation_id_;
  bool did_request_token_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TOOL_INVOCATION_H_
