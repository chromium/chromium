// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_scope.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_work_task.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
const char kInvalidLayoutChild[] = "The LayoutChild is not valid.";
}  // namespace

CustomLayoutChild::CustomLayoutChild(const CSSLayoutDefinition& definition,
                                     LayoutInputNode node)
    : node_(node),
      style_map_(MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          node.GetDocument(),
          node.Style(),
          definition.ChildNativeInvalidationProperties(),
          definition.ChildCustomInvalidationProperties())) {}

ScriptPromise<CustomIntrinsicSizes> CustomLayoutChild::intrinsicSizes(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // A layout child may be invalid if it has been removed from the tree (it is
  // possible for a web developer to hold onto a LayoutChild object after its
  // underlying LayoutObject has been destroyed).
  if (!node_ || !token_->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidLayoutChild);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<CustomIntrinsicSizes>>(
          script_state, exception_state.GetContext());
  CustomLayoutScope::Current()->Queue()->emplace_back(
      MakeGarbageCollected<CustomLayoutWorkTask>(
          this, token_, resolver,
          CustomLayoutWorkTask::TaskType::kIntrinsicSizes));
  return resolver->Promise();
}

ScriptPromise<CustomLayoutFragment> CustomLayoutChild::layoutNextFragment(
    ScriptState* script_state,
    const CustomLayoutConstraintsOptions* options,
    ExceptionState& exception_state) {
  // A layout child may be invalid if it has been removed from the tree (it is
  // possible for a web developer to hold onto a LayoutChild object after its
  // underlying LayoutObject has been destroyed).
  if (!node_ || !token_->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidLayoutChild);
    return EmptyPromise();
  }

  // Serialize the provided data if needed.
  scoped_refptr<SerializedScriptValue> constraint_data;
  if (options->hasData()) {
    v8::Local<v8::Value> data = options->data().V8Value();
    // TODO(peria): Remove this branch.  We don't serialize null values for
    // backward compatibility.  https://crbug.com/1070871
    if (!data->IsNullOrUndefined()) {
      // We serialize "kForStorage" so that SharedArrayBuffers can't be shared
      // between LayoutWorkletGlobalScopes.
      constraint_data = SerializedScriptValue::Serialize(
          script_state->GetIsolate(), data,
          SerializedScriptValue::SerializeOptions(
              SerializedScriptValue::kForStorage),
          exception_state);

      if (exception_state.HadException())
        return EmptyPromise();
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<CustomLayoutFragment>>(
          script_state, exception_state.GetContext());
  CustomLayoutScope::Current()->Queue()->emplace_back(
      MakeGarbageCollected<CustomLayoutWorkTask>(
          this, token_, resolver, options, std::move(constraint_data),
          CustomLayoutWorkTask::TaskType::kLayoutFragment));
  return resolver->Promise();
}

void CustomLayoutChild::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(style_map_);
  visitor->Trace(token_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
