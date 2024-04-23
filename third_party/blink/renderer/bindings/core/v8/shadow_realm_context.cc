// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/shadow_realm_context.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "v8/include/v8-context.h"

namespace blink {

namespace {

// This is a helper class to make the initiator ExecutionContext the owner
// of a ShadowRealmGlobalScope and its ScriptState. When the initiator
// ExecutionContext is destroyed, the ShadowRealmGlobalScope is destroyed,
// too.
class ShadowRealmLifetimeController
    : public GarbageCollected<ShadowRealmLifetimeController>,
      public ContextLifecycleObserver {
 public:
  explicit ShadowRealmLifetimeController(
      ExecutionContext* initiator_execution_context,
      ShadowRealmGlobalScope* shadow_realm_global_scope,
      ScriptState* shadow_realm_script_state)
      : is_initiator_worker_or_worklet_(
            initiator_execution_context->IsWorkerOrWorkletGlobalScope()),
        shadow_realm_global_scope_(shadow_realm_global_scope),
        shadow_realm_script_state_(shadow_realm_script_state) {
    SetContextLifecycleNotifier(initiator_execution_context);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(shadow_realm_global_scope_);
    visitor->Trace(shadow_realm_script_state_);
    ContextLifecycleObserver::Trace(visitor);
  }

 protected:
  void ContextDestroyed() override {
    shadow_realm_script_state_->DisposePerContextData();
    if (is_initiator_worker_or_worklet_) {
      shadow_realm_script_state_->DissociateContext();
    }
    shadow_realm_script_state_.Clear();
    shadow_realm_global_scope_->NotifyContextDestroyed();
    shadow_realm_global_scope_.Clear();
  }

 private:
  bool is_initiator_worker_or_worklet_;
  Member<ShadowRealmGlobalScope> shadow_realm_global_scope_;
  Member<ScriptState> shadow_realm_script_state_;
};

}  // namespace

v8::MaybeLocal<v8::Context> OnCreateShadowRealmV8Context(
    v8::Local<v8::Context> initiator_context) {
  ExecutionContext* initiator_execution_context =
      ExecutionContext::From(initiator_context);
  DCHECK(initiator_execution_context);
  v8::Isolate* isolate = initiator_context->GetIsolate();
  DOMWrapperWorld* world = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kShadowRealm);
  CHECK(world);  // Not yet run out of the world id.

  // Create a new ShadowRealmGlobalScope.
  ShadowRealmGlobalScope* shadow_realm_global_scope =
      MakeGarbageCollected<ShadowRealmGlobalScope>(initiator_execution_context);
  const WrapperTypeInfo* wrapper_type_info =
      shadow_realm_global_scope->GetWrapperTypeInfo();

  // Create a new v8::Context.
  v8::ExtensionConfiguration* extension_configuration = nullptr;
  v8::Local<v8::ObjectTemplate> global_template =
      wrapper_type_info->GetV8ClassTemplate(isolate, *world)
          .As<v8::FunctionTemplate>()
          ->InstanceTemplate();
  v8::Local<v8::Object> global_proxy;  // Will request a new global proxy.
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, extension_configuration, global_template,
                       global_proxy, v8::DeserializeInternalFieldsCallback(),
                       initiator_execution_context->GetMicrotaskQueue());
  context->UseDefaultSecurityToken();

  // Associate the Blink object with the v8::Context.
  ScriptState* script_state =
      ScriptState::Create(context, world, shadow_realm_global_scope);

  // Associate the Blink object with the v8::Objects.
  global_proxy = context->Global();
  V8DOMWrapper::SetNativeInfo(isolate, global_proxy, shadow_realm_global_scope);
  v8::Local<v8::Object> global_object =
      global_proxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::SetNativeInfo(isolate, global_object,
                              shadow_realm_global_scope);

  // Install context-dependent properties.
  std::ignore =
      script_state->PerContextData()->ConstructorForType(wrapper_type_info);

  // Make the initiator execution context the owner of the
  // ShadowRealmGlobalScope and the ScriptState.
  MakeGarbageCollected<ShadowRealmLifetimeController>(
      initiator_execution_context, shadow_realm_global_scope, script_state);

  return context;
}

}  // namespace blink
