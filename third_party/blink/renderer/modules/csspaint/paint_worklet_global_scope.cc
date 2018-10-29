// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_worklet.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"

namespace blink {

namespace {

bool ParseInputArguments(v8::Local<v8::Context> context,
                         v8::Local<v8::Function> constructor,
                         Vector<CSSSyntaxDescriptor>* input_argument_types,
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
        CSSSyntaxDescriptor syntax_descriptor(type);
        if (!syntax_descriptor.IsValid()) {
          exception_state->ThrowTypeError("Invalid argument types.");
          return false;
        }
        input_argument_types->push_back(std::move(syntax_descriptor));
      }
    }
  }
  return true;
}

bool ParsePaintRenderingContext2DSettings(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> constructor,
    PaintRenderingContext2DSettings* context_settings,
    ExceptionState* exception_state) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch block(isolate);

  v8::Local<v8::Value> context_settings_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "contextOptions"))
           .ToLocal(&context_settings_value)) {
    exception_state->RethrowV8Exception(block.Exception());
    return false;
  }
  V8PaintRenderingContext2DSettings::ToImpl(
      isolate, context_settings_value, *context_settings, *exception_state);
  if (exception_state->HadException())
    return false;
  return true;
}

}  // namespace

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry,
    size_t global_scope_number) {
  auto* global_scope =
      new PaintWorkletGlobalScope(frame, std::move(creation_params),
                                  reporting_proxy, pending_generator_registry);
  String context_name("PaintWorklet #");
  context_name.append(String::Number(global_scope_number));
  global_scope->ScriptController()->InitializeContextIfNeeded(context_name,
                                                              NullURL());
  MainThreadDebugger::Instance()->ContextCreated(
      global_scope->ScriptController()->GetScriptState(),
      global_scope->GetFrame(), global_scope->DocumentSecurityOrigin());
  return global_scope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry)
    : WorkletGlobalScope(std::move(creation_params), reporting_proxy, frame),
      pending_generator_registry_(pending_generator_registry) {}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope() = default;

void PaintWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());
  pending_generator_registry_ = nullptr;
  WorkletGlobalScope::Dispose();
}

void PaintWorkletGlobalScope::registerPaint(
    const String& name,
    const ScriptValue& constructor_value,
    ExceptionState& exception_state) {
  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (paint_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(constructor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(constructor_value.V8Value());

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          context, constructor, "inputProperties",
          &native_invalidation_properties, &custom_invalidation_properties,
          &exception_state))
    return;

  // Get input argument types. Parse the argument type values only when
  // cssPaintAPIArguments is enabled.
  Vector<CSSSyntaxDescriptor> input_argument_types;
  if (!ParseInputArguments(context, constructor, &input_argument_types,
                           &exception_state))
    return;

  PaintRenderingContext2DSettings context_settings;
  if (!ParsePaintRenderingContext2DSettings(
          context, constructor, &context_settings, &exception_state))
    return;

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> paint;
  if (!V8ObjectParser::ParseFunction(context, prototype, "paint", &paint,
                                     &exception_state))
    return;

  CSSPaintDefinition* definition = CSSPaintDefinition::Create(
      ScriptController()->GetScriptState(), constructor, paint,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, context_settings);
  paint_definitions_.Set(name, definition);

  // TODO(xidachen): the following steps should be done with a postTask when
  // we move PaintWorklet off main thread.
  PaintWorklet* paint_worklet =
      PaintWorklet::From(*GetFrame()->GetDocument()->domWindow());
  PaintWorklet::DocumentDefinitionMap& document_definition_map =
      paint_worklet->GetDocumentDefinitionMap();
  if (document_definition_map.Contains(name)) {
    DocumentPaintDefinition* existing_document_definition =
        document_definition_map.at(name);
    if (existing_document_definition == kInvalidDocumentPaintDefinition)
      return;
    if (!existing_document_definition->RegisterAdditionalPaintDefinition(
            *definition)) {
      document_definition_map.Set(name, kInvalidDocumentPaintDefinition);
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "A class with name:'" + name +
              "' was registered with a different definition.");
      return;
    }
    // Notify the generator ready only when register paint is called the second
    // time with the same |name| (i.e. there is already a document definition
    // associated with |name|
    if (existing_document_definition->GetRegisteredDefinitionCount() ==
        PaintWorklet::kNumGlobalScopes)
      pending_generator_registry_->NotifyGeneratorReady(name);
  } else {
    DocumentPaintDefinition* document_definition =
        new DocumentPaintDefinition(definition);
    document_definition_map.Set(name, document_definition);
  }
}

CSSPaintDefinition* PaintWorkletGlobalScope::FindDefinition(
    const String& name) {
  return paint_definitions_.at(name);
}

double PaintWorkletGlobalScope::devicePixelRatio() const {
  return GetFrame()->DevicePixelRatio();
}

void PaintWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(paint_definitions_);
  visitor->Trace(pending_generator_registry_);
  WorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
