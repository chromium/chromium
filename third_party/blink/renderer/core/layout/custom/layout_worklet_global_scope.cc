// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intrinsic_sizes_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_layout_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/document_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"

namespace blink {

// static
LayoutWorkletGlobalScope* LayoutWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PendingLayoutRegistry* pending_layout_registry) {
  auto* global_scope = MakeGarbageCollected<LayoutWorkletGlobalScope>(
      frame, std::move(creation_params), reporting_proxy,
      pending_layout_registry);
  global_scope->ScriptController()->Initialize(NullURL());
  MainThreadDebugger::Instance(global_scope->GetIsolate())
      ->ContextCreated(global_scope->ScriptController()->GetScriptState(),
                       global_scope->GetFrame(),
                       global_scope->DocumentSecurityOrigin());
  return global_scope;
}

LayoutWorkletGlobalScope::LayoutWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PendingLayoutRegistry* pending_layout_registry)
    : WorkletGlobalScope(std::move(creation_params), reporting_proxy, frame),
      pending_layout_registry_(pending_layout_registry) {}

LayoutWorkletGlobalScope::~LayoutWorkletGlobalScope() = default;

void LayoutWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance(GetIsolate())
      ->ContextWillBeDestroyed(ScriptController()->GetScriptState());

  WorkletGlobalScope::Dispose();

  NotifyContextDestroyed();
}

// https://drafts.css-houdini.org/css-layout-api/#dom-layoutworkletglobalscope-registerlayout
void LayoutWorkletGlobalScope::registerLayout(
    const AtomicString& name,
    V8NoArgumentConstructor* layout_ctor,
    ExceptionState& exception_state) {
  if (name.empty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (layout_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (!layout_ctor->IsConstructor()) {
    exception_state.ThrowTypeError(
        "The provided callback is not a constructor.");
    return;
  }

  v8::Local<v8::Context> current_context =
      layout_ctor->GetIsolate()->GetCurrentContext();

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          current_context, GetFrame()->DomWindow(),
          layout_ctor->CallbackObject(), AtomicString("inputProperties"),
          &native_invalidation_properties, &custom_invalidation_properties,
          exception_state)) {
    return;
  }

  Vector<CSSPropertyID> child_native_invalidation_properties;
  Vector<AtomicString> child_custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          current_context, GetFrame()->DomWindow(),
          layout_ctor->CallbackObject(), AtomicString("childInputProperties"),
          &child_native_invalidation_properties,
          &child_custom_invalidation_properties, exception_state)) {
    return;
  }

  CallbackMethodRetriever retriever(layout_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException())
    return;

  v8::Local<v8::Function> v8_intrinsic_sizes =
      retriever.GetMethodOrThrow("intrinsicSizes", exception_state);
  if (exception_state.HadException())
    return;
  V8IntrinsicSizesCallback* intrinsic_sizes =
      V8IntrinsicSizesCallback::Create(v8_intrinsic_sizes);

  v8::Local<v8::Function> v8_layout =
      retriever.GetMethodOrThrow("layout", exception_state);
  if (exception_state.HadException())
    return;
  V8LayoutCallback* layout = V8LayoutCallback::Create(v8_layout);

  CSSLayoutDefinition* definition = MakeGarbageCollected<CSSLayoutDefinition>(
      ScriptController()->GetScriptState(), layout_ctor, intrinsic_sizes,
      layout, native_invalidation_properties, custom_invalidation_properties,
      child_native_invalidation_properties,
      child_custom_invalidation_properties);
  layout_definitions_.Set(name, definition);

  LayoutWorklet* layout_worklet = LayoutWorklet::From(*GetFrame()->DomWindow());
  LayoutWorklet::DocumentDefinitionMap* document_definition_map =
      layout_worklet->GetDocumentDefinitionMap();
  if (document_definition_map->Contains(name)) {
    DocumentLayoutDefinition* existing_document_definition =
        document_definition_map->at(name);
    if (existing_document_definition == kInvalidDocumentLayoutDefinition)
      return;
    if (!existing_document_definition->RegisterAdditionalLayoutDefinition(
            *definition)) {
      document_definition_map->Set(name, kInvalidDocumentLayoutDefinition);
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "A class with name:'" + name +
                                            "' was registered with a "
                                            "different definition.");
      return;
    }

    // Notify all of the pending layouts that all of the layout classes with
    // |name| have been registered and are ready to use.
    if (existing_document_definition->GetRegisteredDefinitionCount() ==
        LayoutWorklet::kNumGlobalScopes)
      pending_layout_registry_->NotifyLayoutReady(name);
  } else {
    DocumentLayoutDefinition* document_definition =
        MakeGarbageCollected<DocumentLayoutDefinition>(definition);
    document_definition_map->Set(name, document_definition);
  }
}

CSSLayoutDefinition* LayoutWorkletGlobalScope::FindDefinition(
    const AtomicString& name) {
  return layout_definitions_.at(name);
}

void LayoutWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(layout_definitions_);
  visitor->Trace(pending_layout_registry_);
  WorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
