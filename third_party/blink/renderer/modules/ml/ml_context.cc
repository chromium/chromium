// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MLContext::MLContext(const V8MLDevicePreference device_preference,
                     const V8MLPowerPreference power_preference,
                     const V8MLModelFormat model_format,
                     const unsigned int num_threads,
                     ML* ml)
    : device_preference_(device_preference),
      power_preference_(power_preference),
      model_format_(model_format),
      num_threads_(num_threads),
      ml_(ml),
      webnn_context_(ml->GetExecutionContext()) {}

MLContext::~MLContext() = default;

V8MLDevicePreference MLContext::GetDevicePreference() const {
  return device_preference_;
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
  return ml_model_loader_;
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  visitor->Trace(ml_model_loader_);
  visitor->Trace(webnn_context_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise MLContext::compute(ScriptState* script_state,
                                 MLGraph* graph,
                                 const MLNamedArrayBufferViews& inputs,
                                 const MLNamedArrayBufferViews& outputs,
                                 ExceptionState& exception_state) {
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
    graph->ComputeAsync(inputs, outputs, resolver, exception_state);
  }

  return promise;
}

void MLContext::computeSync(MLGraph* graph,
                            const MLNamedArrayBufferViews& inputs,
                            const MLNamedArrayBufferViews& outputs,
                            ExceptionState& exception_state) {
  if (graph->Context() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The graph isn't built within this context.");
    return;
  }
  graph->ComputeSync(inputs, outputs, exception_state);
}

void MLContext::CreateWebNNGraph(ScriptState* script_state,
                                 CreateWebNNGraphCallback callback) {
  if (!webnn_context_.is_bound()) {
    // Needs to create `WebNNContext` interface first.
    auto options = webnn::mojom::blink::CreateContextOptions::New();
    // TODO(crbug.com/1273291): Set power preference in the context option.
    ml_->CreateWebNNContext(
        std::move(options),
        WTF::BindOnce(&MLContext::OnCreateWebNNContext, WrapPersistent(this),
                      WrapPersistent(script_state), std::move(callback)));
  } else {
    // Directly use `WebNNContext` to create `WebNNGraph` message pipe.
    webnn_context_->CreateGraph(
        WTF::BindOnce(std::move(callback), CreateWebNNGraphResult::kOk));
  }
}

void MLContext::OnCreateWebNNContext(
    ScriptState* script_state,
    CreateWebNNGraphCallback callback,
    webnn::mojom::blink::CreateContextResult result,
    mojo::PendingRemote<webnn::mojom::blink::WebNNContext>
        pending_remote_context) {
  if (!script_state->ContextIsValid()) {
    std::move(callback).Run(CreateWebNNGraphResult::kUnknownError,
                            mojo::NullRemote());
    return;
  }
  switch (result) {
    case webnn::mojom::blink::CreateContextResult::kUnknownError: {
      std::move(callback).Run(CreateWebNNGraphResult::kUnknownError,
                              mojo::NullRemote());
      return;
    }
    case webnn::mojom::blink::CreateContextResult::kNotSupported: {
      std::move(callback).Run(CreateWebNNGraphResult::kNotSupported,
                              mojo::NullRemote());
      return;
    }
    case webnn::mojom::blink::CreateContextResult::kOk: {
      auto* execution_context = ExecutionContext::From(script_state);
      webnn_context_.Bind(
          std::move(pending_remote_context),
          execution_context->GetTaskRunner(TaskType::kInternalDefault));

      webnn_context_->CreateGraph(
          WTF::BindOnce(std::move(callback), CreateWebNNGraphResult::kOk));
      return;
    }
  }
}

}  // namespace blink
