// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_worklet.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

bool ParseInputArguments(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> constructor,
                         Vector<CSSSyntaxDefinition>* input_argument_types,
                         ExceptionState* exception_state) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch block(isolate);

  if (RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    v8::Local<v8::Value> input_argument_type_values;
    if (!constructor->Get(context, V8AtomicString(isolate, "inputArguments"))
             .ToLocal(&input_argument_type_values)) {
      exception_state->RethrowV8Exception(block.Exception());
      return false;
    }

    if (!input_argument_type_values->IsNullOrUndefined()) {
      Vector<String> argument_types =
          NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
              isolate, input_argument_type_values, *exception_state);

      if (exception_state->HadException())
        return false;

      for (const auto& type : argument_types) {
        base::Optional<CSSSyntaxDefinition> syntax_definition =
            CSSSyntaxStringParser(type).Parse();
        if (!syntax_definition) {
          exception_state->ThrowTypeError("Invalid argument types.");
          return false;
        }
        input_argument_types->push_back(std::move(*syntax_definition));
      }
    }
  }
  return true;
}

PaintRenderingContext2DSettings* ParsePaintRenderingContext2DSettings(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> constructor,
    ExceptionState* exception_state) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch block(isolate);

  v8::Local<v8::Value> context_settings_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "contextOptions"))
           .ToLocal(&context_settings_value)) {
    exception_state->RethrowV8Exception(block.Exception());
    return nullptr;
  }
  auto* context_settings =
      NativeValueTraits<PaintRenderingContext2DSettings>::NativeValue(
          isolate, context_settings_value, *exception_state);
  if (exception_state->HadException())
    return nullptr;
  return context_settings;
}

}  // namespace

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy) {
  auto* global_scope = MakeGarbageCollected<PaintWorkletGlobalScope>(
      frame, std::move(creation_params), reporting_proxy);
  global_scope->ScriptController()->Initialize(NullURL());
  MainThreadDebugger::Instance()->ContextCreated(
      global_scope->ScriptController()->GetScriptState(),
      global_scope->GetFrame(), global_scope->DocumentSecurityOrigin());
  return global_scope;
}

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread) {
  DCHECK(RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled());
  return MakeGarbageCollected<PaintWorkletGlobalScope>(
      std::move(creation_params), thread);
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy)
    : WorkletGlobalScope(std::move(creation_params), reporting_proxy, frame) {}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope() = default;

void PaintWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  if (!WTF::IsMainThread()) {
    if (PaintWorkletProxyClient* proxy_client =
            PaintWorkletProxyClient::From(Clients()))
      proxy_client->Dispose();
  } else {
    MainThreadDebugger::Instance()->ContextWillBeDestroyed(
        ScriptController()->GetScriptState());
  }
  WorkletGlobalScope::Dispose();
}

void PaintWorkletGlobalScope::registerPaint(const String& name,
                                            V8NoArgumentConstructor* paint_ctor,
                                            ExceptionState& exception_state) {
  // https://drafts.css-houdini.org/css-paint-api/#dom-paintworkletglobalscope-registerpaint

  RegisterWithProxyClientIfNeeded();

  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (paint_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (!paint_ctor->IsConstructor()) {
    exception_state.ThrowTypeError(
        "The provided callback is not a constructor.");
    return;
  }

  v8::Local<v8::Context> context = ScriptController()->GetContext();

  v8::Local<v8::Object> v8_paint_ctor = paint_ctor->CallbackObject();

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          context, v8_paint_ctor, "inputProperties",
          &native_invalidation_properties, &custom_invalidation_properties,
          &exception_state))
    return;

  // Get input argument types. Parse the argument type values only when
  // cssPaintAPIArguments is enabled.
  Vector<CSSSyntaxDefinition> input_argument_types;
  if (!ParseInputArguments(context, v8_paint_ctor, &input_argument_types,
                           &exception_state))
    return;

  PaintRenderingContext2DSettings* context_settings =
      ParsePaintRenderingContext2DSettings(context, v8_paint_ctor,
                                           &exception_state);
  if (!context_settings)
    return;

  CallbackMethodRetriever retriever(paint_ctor);

  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException())
    return;

  v8::Local<v8::Function> v8_paint =
      retriever.GetMethodOrThrow("paint", exception_state);
  if (exception_state.HadException())
    return;
  V8PaintCallback* paint = V8PaintCallback::Create(v8_paint);

  auto* definition = MakeGarbageCollected<CSSPaintDefinition>(
      ScriptController()->GetScriptState(), paint_ctor, paint,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, context_settings);
  paint_definitions_.Set(name, definition);

  if (!WTF::IsMainThread()) {
    PaintWorkletProxyClient* proxy_client =
        PaintWorkletProxyClient::From(Clients());
    proxy_client->RegisterCSSPaintDefinition(name, definition, exception_state);
  } else {
    PaintWorklet* paint_worklet =
        PaintWorklet::From(*GetFrame()->GetDocument()->domWindow());
    paint_worklet->RegisterCSSPaintDefinition(name, definition,
                                              exception_state);
  }
}

CSSPaintDefinition* PaintWorkletGlobalScope::FindDefinition(
    const String& name) {
  return paint_definitions_.at(name);
}

double PaintWorkletGlobalScope::devicePixelRatio() const {
  return WTF::IsMainThread()
             ? GetFrame()->DevicePixelRatio()
             : PaintWorkletProxyClient::From(Clients())->DevicePixelRatio();
}

void PaintWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(paint_definitions_);
  WorkletGlobalScope::Trace(visitor);
}

void PaintWorkletGlobalScope::RegisterWithProxyClientIfNeeded() {
  if (registered_ || WTF::IsMainThread())
    return;

  if (PaintWorkletProxyClient* proxy_client =
          PaintWorkletProxyClient::From(Clients())) {
    proxy_client->AddGlobalScope(this);
    registered_ = true;
  }
}

}  // namespace blink
