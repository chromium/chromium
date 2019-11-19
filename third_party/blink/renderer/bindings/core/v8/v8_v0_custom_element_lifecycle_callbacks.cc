/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_v0_custom_element_lifecycle_callbacks.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/v0_custom_element_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

namespace blink {

#define CALLBACK_LIST(V)        \
  V(created, CreatedCallback)   \
  V(attached, AttachedCallback) \
  V(detached, DetachedCallback) \
  V(attribute_changed, AttributeChangedCallback)

static V0CustomElementLifecycleCallbacks::CallbackType FlagSet(
    v8::MaybeLocal<v8::Function> attached,
    v8::MaybeLocal<v8::Function> detached,
    v8::MaybeLocal<v8::Function> attribute_changed) {
  // V8 Custom Elements always run created to swizzle prototypes.
  int flags = V0CustomElementLifecycleCallbacks::kCreatedCallback;

  if (!attached.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kAttachedCallback;

  if (!detached.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kDetachedCallback;

  if (!attribute_changed.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kAttributeChangedCallback;

  return V0CustomElementLifecycleCallbacks::CallbackType(flags);
}

V8V0CustomElementLifecycleCallbacks::V8V0CustomElementLifecycleCallbacks(
    ScriptState* script_state,
    v8::Local<v8::Object> prototype,
    v8::MaybeLocal<v8::Function> created,
    v8::MaybeLocal<v8::Function> attached,
    v8::MaybeLocal<v8::Function> detached,
    v8::MaybeLocal<v8::Function> attribute_changed)
    : V0CustomElementLifecycleCallbacks(
          FlagSet(attached, detached, attribute_changed)),
      script_state_(script_state),
      prototype_(script_state->GetIsolate(), prototype){
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Function> function;

// A given object can only be used as a Custom Element prototype
// once; see customElementIsInterfacePrototypeObject
#define SET_PRIVATE_PROPERTY(Maybe, Name)                            \
  static const V8PrivateProperty::SymbolKey kPrivateProperty##Name;  \
  V8PrivateProperty::Symbol symbol##Name =                           \
      V8PrivateProperty::GetSymbol(isolate, kPrivateProperty##Name); \
  DCHECK(!symbol##Name.HasValue(prototype));                         \
  {                                                                  \
    if (Maybe.ToLocal(&function))                                    \
      symbol##Name.Set(prototype, function);                         \
  }

  CALLBACK_LIST(SET_PRIVATE_PROPERTY)
#undef SET_PRIVATE_PROPERTY

#define SET_FIELD(maybe, ignored)    \
  if (maybe.ToLocal(&function))      \
    maybe##_.Set(isolate, function);

  CALLBACK_LIST(SET_FIELD)
#undef SET_FIELD
}

V8PerContextData* V8V0CustomElementLifecycleCallbacks::CreationContextData() {
  if (!script_state_->ContextIsValid())
    return nullptr;

  v8::Local<v8::Context> context = script_state_->GetContext();
  if (context.IsEmpty())
    return nullptr;

  return V8PerContextData::From(context);
}

V8V0CustomElementLifecycleCallbacks::~V8V0CustomElementLifecycleCallbacks() =
    default;

bool V8V0CustomElementLifecycleCallbacks::SetBinding(
    std::unique_ptr<V0CustomElementBinding> binding) {
  V8PerContextData* per_context_data = CreationContextData();
  if (!per_context_data)
    return false;

  // The context is responsible for keeping the prototype
  // alive. This in turn keeps callbacks alive through hidden
  // references; see CALLBACK_LIST(SET_HIDDEN_VALUE).
  per_context_data->AddCustomElementBinding(std::move(binding));
  return true;
}

void V8V0CustomElementLifecycleCallbacks::Created(Element* element) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;

  element->SetV0CustomElementState(Element::kV0Upgraded);

  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> receiver_value =
      ToV8(element, context->Global(), isolate);
  if (receiver_value.IsEmpty())
    return;
  v8::Local<v8::Object> receiver = receiver_value.As<v8::Object>();

  // Swizzle the prototype of the wrapper.
  v8::Local<v8::Object> prototype = prototype_.NewLocal(isolate);
  bool set_prototype;
  if (prototype.IsEmpty() ||
      !receiver->SetPrototype(context, prototype).To(&set_prototype) ||
      !set_prototype) {
    return;
  }

  v8::Local<v8::Function> callback = created_.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback, ExecutionContext::From(script_state_),
                               receiver, 0, nullptr, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Attached(Element* element) {
  Call(attached_, element);
}

void V8V0CustomElementLifecycleCallbacks::Detached(Element* element) {
  Call(detached_, element);
}

void V8V0CustomElementLifecycleCallbacks::AttributeChanged(
    Element* element,
    const AtomicString& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> receiver = ToV8(element, context->Global(), isolate);
  if (receiver.IsEmpty())
    return;

  v8::Local<v8::Function> callback = attribute_changed_.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::Local<v8::Value> argv[] = {
      V8String(isolate, name),
      old_value.IsNull() ? v8::Local<v8::Value>(v8::Null(isolate))
                         : v8::Local<v8::Value>(V8String(isolate, old_value)),
      new_value.IsNull() ? v8::Local<v8::Value>(v8::Null(isolate))
                         : v8::Local<v8::Value>(V8String(isolate, new_value))};

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback, ExecutionContext::From(script_state_),
                               receiver, base::size(argv), argv, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Call(
    const TraceWrapperV8Reference<v8::Function>& callback_reference,
    Element* element) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Function> callback = callback_reference.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::Local<v8::Value> receiver = ToV8(element, context->Global(), isolate);
  if (receiver.IsEmpty())
    return;

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback, ExecutionContext::From(script_state_),
                               receiver, 0, nullptr, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(prototype_);
  visitor->Trace(created_);
  visitor->Trace(attached_);
  visitor->Trace(detached_);
  visitor->Trace(attribute_changed_);
  V0CustomElementLifecycleCallbacks::Trace(visitor);
}

}  // namespace blink
