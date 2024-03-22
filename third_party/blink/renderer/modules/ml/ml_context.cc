// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "base/notreached.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer_mojo.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error_mojo.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

webnn::mojom::blink::PowerPreference ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kAuto:
      return webnn::mojom::blink::PowerPreference::kDefault;
    case V8MLPowerPreference::Enum::kLowPower:
      return webnn::mojom::blink::PowerPreference::kLowPower;
    case V8MLPowerPreference::Enum::kHighPerformance:
      return webnn::mojom::blink::PowerPreference::kHighPerformance;
  }
}

}  // namespace

// static
void MLContext::ValidateAndCreate(
    ScriptPromiseResolverTyped<MLContext>* resolver,
    MLContextOptions* options,
    ML* ml) {
  ScopedMLTrace scoped_trace("MLContext::ValidateAndCreate");
  auto* context = MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->deviceType(),
      options->powerPreference(), options->modelFormat(), options->numThreads(),
      ml);

  // TODO: crbug.com/325612086 - The WebNN Service supports CPU execution via
  // TFLite, but that code path is currently only hit when asking a "gpu"
  // context for the sake of testing. This should be fixed.
  if (options->deviceType() == V8MLDeviceType::Enum::kGpu) {
    auto options_mojo = webnn::mojom::blink::CreateContextOptions::New(
        ConvertBlinkPowerPreferenceToMojo(options->powerPreference()));
    ml->CreateWebNNContext(
        std::move(options_mojo),
        WTF::BindOnce(&MLContext::OnCreateWebNNContext, WrapPersistent(context),
                      std::move(scoped_trace), WrapPersistent(resolver)));
    return;
  }

  resolver->Resolve(context);
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
      ml_(ml),
      remote_context_(ml->GetExecutionContext()) {}

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

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  visitor->Trace(remote_context_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromiseTyped<MLComputeResult> MLContext::compute(
    ScriptState* script_state,
    MLGraph* graph,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::compute");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromiseTyped<MLComputeResult>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolverTyped<MLComputeResult>>(
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

void MLContext::CreateWebNNGraph(
    webnn::mojom::blink::GraphInfoPtr graph_info,
    webnn::mojom::blink::WebNNContext::CreateGraphCallback callback) {
  if (!remote_context_.is_bound()) {
    std::move(callback).Run(webnn::mojom::blink::CreateGraphResult::NewError(
        webnn::mojom::blink::Error::New(
            webnn::mojom::blink::Error::Code::kUnknownError,
            "Invalid script state.")));
    return;
  }

  remote_context_->CreateGraph(std::move(graph_info),
                               WTF::BindOnce(std::move(callback)));
}

void MLContext::OnCreateWebNNContext(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolverTyped<MLContext>* resolver,
    webnn::mojom::blink::CreateContextResultPtr result) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state) {
    return;
  }

  if (result->is_error()) {
    const auto& create_context_error = result->get_error();
    resolver->RejectWithDOMException(
        ConvertWebNNErrorCodeToDOMExceptionCode(create_context_error->code),
        create_context_error->message);
    return;
  }

  remote_context_.Bind(std::move(result->get_context_remote()),
                       ExecutionContext::From(script_state)
                           ->GetTaskRunner(TaskType::kMiscPlatformAPI));

  resolver->Resolve(this);
}

void MLContext::CreateWebNNBuffer(
    mojo::PendingReceiver<webnn::mojom::blink::WebNNBuffer> receiver,
    webnn::mojom::blink::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_context_.is_bound()) {
    return;
  }

  // Use `WebNNContext` to create `WebNNBuffer` message pipe.
  remote_context_->CreateBuffer(std::move(receiver), std::move(buffer_info),
                                buffer_handle);
}

MLBuffer* MLContext::createBuffer(ScriptState* script_state,
                                  const MLBufferDescriptor* descriptor,
                                  ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::createBuffer");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

  // TODO: crbug.com/325612086 - The WebNN Service supports CPU execution via
  // TFLite, but that code path is currently only hit when asking a "gpu"
  // context for the sake of testing. This should be fixed.
  if (device_type_ == V8MLDeviceType::Enum::kGpu) {
    return MLBufferMojo::Create(std::move(scoped_trace), script_state, this,
                                descriptor, exception_state);
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}
}  // namespace blink
