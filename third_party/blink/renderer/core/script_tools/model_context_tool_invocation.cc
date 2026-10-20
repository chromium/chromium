// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_tool_invocation.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"

namespace blink {

ModelContextToolInvocation::ModelContextToolInvocation(
    ModelContext* model_context,
    const ToolInvocationToken& invocation_id)
    : model_context_(model_context), invocation_id_(invocation_id) {}

ScriptPromise<IDLString> ModelContextToolInvocation::requestToken(
    ScriptState* script_state) {
  return model_context_->RequestContinuationToken(
      base::PassKey<ModelContextToolInvocation>(), script_state, invocation_id_,
      did_request_token_);
}

void ModelContextToolInvocation::Trace(Visitor* visitor) const {
  visitor->Trace(model_context_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
