// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/css_layout_definition.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fragment_result_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_layout_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_constraints.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_edges.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_scope.h"
#include "third_party/blink/renderer/core/layout/ng/custom/fragment_result_options.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"

namespace blink {

CSSLayoutDefinition::CSSLayoutDefinition(
    ScriptState* script_state,
    V8NoArgumentConstructor* constructor,
    V8Function* intrinsic_sizes,
    V8LayoutCallback* layout,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSPropertyID>& child_native_invalidation_properties,
    const Vector<AtomicString>& child_custom_invalidation_properties)
    : script_state_(script_state),
      constructor_(constructor),
      unused_intrinsic_sizes_(intrinsic_sizes),
      layout_(layout),
      native_invalidation_properties_(native_invalidation_properties),
      custom_invalidation_properties_(custom_invalidation_properties),
      child_native_invalidation_properties_(
          child_native_invalidation_properties),
      child_custom_invalidation_properties_(
          child_custom_invalidation_properties) {}

CSSLayoutDefinition::~CSSLayoutDefinition() = default;

CSSLayoutDefinition::Instance::Instance(CSSLayoutDefinition* definition,
                                        v8::Local<v8::Value> instance)
    : definition_(definition),
      instance_(definition->GetScriptState()->GetIsolate(), instance) {}

bool CSSLayoutDefinition::Instance::Layout(
    const NGConstraintSpace& space,
    const Document& document,
    const NGBlockNode& node,
    const LogicalSize& border_box_size,
    const NGBoxStrut& border_scrollbar_padding,
    CustomLayoutScope* custom_layout_scope,
    FragmentResultOptions* fragment_result_options,
    scoped_refptr<SerializedScriptValue>* fragment_result_data) {
  ScriptState* script_state = definition_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();

  if (!script_state->ContextIsValid())
    return false;

  ScriptState::Scope scope(script_state);

  // TODO(ikilpatrick): Determine if knowing the size of the array ahead of
  // time improves performance in any noticeable way.
  HeapVector<Member<CustomLayoutChild>> children;
  for (NGLayoutInputNode child = node.FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;

    CustomLayoutChild* layout_child = child.GetCustomLayoutChild();
    layout_child->SetCustomLayoutToken(custom_layout_scope->Token());
    DCHECK(layout_child);
    children.push_back(layout_child);
  }

  CustomLayoutEdges* edges =
      MakeGarbageCollected<CustomLayoutEdges>(border_scrollbar_padding);

  CustomLayoutConstraints* constraints =
      MakeGarbageCollected<CustomLayoutConstraints>(
          border_box_size, space.CustomLayoutData(), isolate);

  // TODO(ikilpatrick): Instead of creating a new style_map each time here,
  // store on LayoutCustom, and update when the style changes.
  StylePropertyMapReadOnly* style_map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          document, node.Style(), definition_->native_invalidation_properties_,
          definition_->custom_invalidation_properties_);

  ScriptValue return_value;
  if (!definition_->layout_
           ->Invoke(instance_.NewLocal(isolate), children, edges, constraints,
                    style_map)
           .To(&return_value))
    return false;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::MicrotaskQueue* microtask_queue = ToMicrotaskQueue(execution_context);
  DCHECK(microtask_queue);

  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "CSSLayoutAPI", "Layout");

  v8::Local<v8::Value> v8_return_value = return_value.V8Value();
  if (v8_return_value.IsEmpty() || !v8_return_value->IsPromise()) {
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "The layout function must be async or return a "
                               "promise, falling back to block layout."));
    return false;
  }

  // Run the work queue until exhaustion.
  while (!custom_layout_scope->Queue()->IsEmpty()) {
    for (auto& task : *custom_layout_scope->Queue())
      task.Run(space, node.Style());
    custom_layout_scope->Queue()->clear();
    {
      v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                           v8::MicrotasksScope::kRunMicrotasks);
    }
  }

  if (exception_state.HadException()) {
    ReportException(&exception_state);
    return false;
  }

  v8::Local<v8::Promise> v8_result_promise =
      v8::Local<v8::Promise>::Cast(v8_return_value);

  if (v8_result_promise->State() != v8::Promise::kFulfilled) {
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "The layout function promise must resolve, "
                               "falling back to block layout."));
    return false;
  }
  v8::Local<v8::Value> inner_value = v8_result_promise->Result();

  // Attempt to convert the result.
  V8FragmentResultOptions::ToImpl(isolate, inner_value, fragment_result_options,
                                  exception_state);

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Unable to parse the layout function "
                               "result, falling back to block layout."));
    return false;
  }

  // Serialize any extra data provided by the web-developer to potentially pass
  // up to the parent custom layout.
  if (fragment_result_options->hasData()) {
    // We serialize "kForStorage" so that SharedArrayBuffers can't be shared
    // between LayoutWorkletGlobalScopes.
    *fragment_result_data = SerializedScriptValue::Serialize(
        isolate, fragment_result_options->data().V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kForStorage),
        exception_state);
  }

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Unable to serialize the data provided in the "
                               "result, falling back to block layout."));
    return false;
  }

  return true;
}

void CSSLayoutDefinition::Instance::ReportException(
    ExceptionState* exception_state) {
  ScriptState* script_state = definition_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // We synchronously report and clear the exception as we may never enter V8
  // again (as the callbacks are invoked directly by the UA).
  V8ScriptRunner::ReportException(isolate, exception_state->GetException());
  exception_state->ClearException();
  execution_context->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kInfo,
      "The layout function failed, falling back to block layout."));
}

CSSLayoutDefinition::Instance* CSSLayoutDefinition::CreateInstance() {
  if (constructor_has_failed_)
    return nullptr;

  // Ensure that we don't create an instance on a detached context.
  if (!GetScriptState()->ContextIsValid())
    return nullptr;

  ScriptState::Scope scope(GetScriptState());

  ScriptValue instance;
  if (!constructor_->Construct().To(&instance)) {
    constructor_has_failed_ = true;
    return nullptr;
  }

  return MakeGarbageCollected<Instance>(this, instance.V8Value());
}

void CSSLayoutDefinition::Instance::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
  visitor->Trace(instance_);
}

void CSSLayoutDefinition::Trace(Visitor* visitor) {
  visitor->Trace(constructor_);
  visitor->Trace(unused_intrinsic_sizes_);
  visitor->Trace(layout_);
  visitor->Trace(script_state_);
}

}  // namespace blink
