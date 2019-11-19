// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_constraints.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

CustomLayoutConstraints::CustomLayoutConstraints(
    const LogicalSize& border_box_size,
    SerializedScriptValue* data,
    v8::Isolate* isolate)
    : fixed_inline_size_(border_box_size.inline_size),
      fixed_block_size_(border_box_size.block_size) {
  if (data)
    layout_worklet_world_v8_data_.Set(isolate, data->Deserialize(isolate));
}

CustomLayoutConstraints::~CustomLayoutConstraints() = default;

double CustomLayoutConstraints::fixedBlockSize(bool& is_null) const {
  // Check if we've been passed an indefinite block-size.
  if (fixed_block_size_ < 0.0) {
    is_null = true;
    return 0.0;
  }

  return fixed_block_size_;
}

ScriptValue CustomLayoutConstraints::data(ScriptState* script_state) const {
  // "data" is *only* exposed to the LayoutWorkletGlobalScope, and we are able
  // to return the same deserialized object. We don't need to check which world
  // it is being accessed from.
  DCHECK(ExecutionContext::From(script_state)->IsLayoutWorkletGlobalScope());
  DCHECK(script_state->World().IsWorkerWorld());

  if (layout_worklet_world_v8_data_.IsEmpty())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  return ScriptValue(
      script_state->GetIsolate(),
      layout_worklet_world_v8_data_.NewLocal(script_state->GetIsolate()));
}

void CustomLayoutConstraints::Trace(blink::Visitor* visitor) {
  visitor->Trace(layout_worklet_world_v8_data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
