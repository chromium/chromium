// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
void MLContext::ValidateAndCreate(ScriptPromiseResolver* resolver,
                                  MLContextOptions* options,
                                  ML* ml) {
  // Notice that currently, we just create the context in the renderer. In the
  // future we may add backend query ability to check whether a context is
  // supportable or not. At that time, this function will be truly asynced.
  //
  // TODO(crbug.com/1273291): Support async context creation for all contexts.
  resolver->Resolve(MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->deviceType(),
      options->powerPreference(), options->modelFormat(), options->numThreads(),
      ml));
}

MLContext::MLContext(const V8MLDevicePreference device_preference,
                     const V8MLDeviceType device_type,
                     const V8MLPowerPreference power_preference,
                     const V8MLModelFormat model_format,
                     const unsigned int num_threads,
                     ML* ml)
    : device_preference_(device_preference),
      device_type_(device_type),
      power_preference_(power_preference),
      model_format_(model_format),
      num_threads_(num_threads),
      ml_(ml) {}

MLContext::~MLContext() = default;

V8MLDevicePreference MLContext::GetDevicePreference() const {
  return device_preference_;
}

V8MLDeviceType MLContext::GetDeviceType() const {
  return device_type_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

V8MLModelFormat MLContext::GetModelFormat() const {
  return model_format_;
}

unsigned int MLContext::GetNumThreads() const {
  return num_threads_;
}

void MLContext::LogConsoleWarning(const String& message) {
  auto* execution_context = ml_->GetExecutionContext();
  if (!execution_context) {
    return;
  }
  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

ML* MLContext::GetML() {
  return ml_.Get();
}

MLModelLoader* MLContext::GetModelLoaderForWebNN(ScriptState* script_state) {
  if (!ml_model_loader_) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    ml_model_loader_ =
        MakeGarbageCollected<MLModelLoader>(execution_context, this);
  }
  return ml_model_loader_.Get();
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  visitor->Trace(ml_model_loader_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise MLContext::compute(ScriptState* script_state,
                                 MLGraph* graph,
                                 const MLNamedArrayBufferViews& inputs,
                                 const MLNamedArrayBufferViews& outputs,
                                 ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::compute");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (graph->Context() != this) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "The graph isn't built within this context."));
  } else {
    graph->Compute(std::move(scoped_trace), inputs, outputs, resolver,
                   exception_state);
  }

  return promise;
}

void MLContext::Create(ScopedMLTrace scoped_trace,
                       ScriptPromiseResolver* resolver,
                       MLContextOptions* options) {
  CreateImpl(std::move(scoped_trace), resolver, options);
}

void MLContext::CreateImpl(ScopedMLTrace scoped_trace,
                           ScriptPromiseResolver* resolver,
                           MLContextOptions* options) {
  // TODO(crbug.com/1273291): Remove when creation gets implemented for all
  // context types.
  NOTIMPLEMENTED();
}

}  // namespace blink
