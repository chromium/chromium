// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"

namespace blink {

CustomLayoutFragment::CustomLayoutFragment(
    CustomLayoutChild* child,
    CustomLayoutToken* token,
    const LayoutResult* layout_result,
    const LogicalSize& size,
    const std::optional<LayoutUnit> baseline,
    v8::Isolate* isolate)
    : child_(child),
      token_(token),
      layout_result_(std::move(layout_result)),
      inline_size_(size.inline_size.ToDouble()),
      block_size_(size.block_size.ToDouble()),
      baseline_(baseline) {
  // Immediately store the result data, so that it remains immutable between
  // layout calls to the child.
  if (SerializedScriptValue* data = layout_result_->CustomLayoutData())
    layout_worklet_world_v8_data_.Reset(isolate, data->Deserialize(isolate));
}

const LayoutResult& CustomLayoutFragment::GetLayoutResult() const {
  DCHECK(layout_result_);
  return *layout_result_;
}

const LayoutInputNode& CustomLayoutFragment::GetLayoutNode() const {
  return child_->GetLayoutNode();
}

ScriptValue CustomLayoutFragment::data(ScriptState* script_state) const {
  // "data" is *only* exposed to the LayoutWorkletGlobalScope, and we are able
  // to return the same deserialized object. We don't need to check which world
  // it is being accessed from.
  DCHECK(ExecutionContext::From(script_state)->IsLayoutWorkletGlobalScope());
  DCHECK(script_state->World().IsWorkerOrWorkletWorld());

  if (layout_worklet_world_v8_data_.IsEmpty())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  return ScriptValue(
      script_state->GetIsolate(),
      layout_worklet_world_v8_data_.Get(script_state->GetIsolate()));
}

void CustomLayoutFragment::Trace(Visitor* visitor) const {
  visitor->Trace(child_);
  visitor->Trace(token_);
  visitor->Trace(layout_result_);
  visitor->Trace(layout_worklet_world_v8_data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
