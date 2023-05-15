// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml.h"

namespace blink {

// static
void MLGraphMojo::ValidateAndBuildAsync(MLContext* context,
                                        const MLNamedOperands& named_outputs,
                                        ScriptPromiseResolver* resolver) {
  auto* graph =
      MakeGarbageCollected<MLGraphMojo>(resolver->GetScriptState(), context);
  graph->BuildAsync(named_outputs, resolver);
}

MLGraphMojo::MLGraphMojo(ScriptState* script_state, MLContext* context)
    : MLGraph(context), remote_graph_(ExecutionContext::From(script_state)) {}

MLGraphMojo::~MLGraphMojo() = default;

void MLGraphMojo::Trace(Visitor* visitor) const {
  visitor->Trace(remote_graph_);
  MLGraph::Trace(visitor);
}

void MLGraphMojo::BuildAsyncImpl(const MLNamedOperands& outputs,
                                 ScriptPromiseResolver* resolver) {
  auto* named_outputs = MakeGarbageCollected<MLNamedOperands>(outputs);
  // Create `WebNNGraph` message pipe with `WebNNContext` mojo interface.
  auto* script_state = resolver->GetScriptState();
  ml_context_->CreateWebNNGraph(
      script_state,
      WTF::BindOnce(&MLGraphMojo::OnCreateWebNNGraph, WrapPersistent(this),
                    WrapPersistent(resolver), WrapPersistent(named_outputs)));
}

MLGraph* MLGraphMojo::BuildSyncImpl(const MLNamedOperands& named_outputs,
                                    ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support sync build that is only exposed to
  // dedicated worker.
  NOTIMPLEMENTED();
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Sync build not implemented.");
  return nullptr;
}

void MLGraphMojo::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                   const MLNamedArrayBufferViews& outputs,
                                   ScriptPromiseResolver* resolver,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support async compute.
  NOTIMPLEMENTED();
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Async compute not implemented."));
}

void MLGraphMojo::ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                                  const MLNamedArrayBufferViews& outputs,
                                  ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support sync compute that is only exposed to
  // dedicated worker.
  NOTIMPLEMENTED();
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Sync compute not implemented");
}

void MLGraphMojo::OnCreateWebNNGraph(
    ScriptPromiseResolver* resolver,
    const MLNamedOperands* named_output,
    MLContext::CreateWebNNGraphResult result,
    mojo::PendingRemote<webnn::mojom::blink::WebNNGraph> pending_remote) {
  switch (result) {
    case MLContext::CreateWebNNGraphResult::kUnknownError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Internal error."));
      return;
    }
    case MLContext::CreateWebNNGraphResult::kNotSupported: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Input configuration not supported."));
      return;
    }
    case MLContext::CreateWebNNGraphResult::kOk: {
      auto* script_state = resolver->GetScriptState();
      auto* execution_context = ExecutionContext::From(script_state);
      // Bind the end point of `WebNNGraph` mojo interface in the blink side.
      remote_graph_.Bind(
          std::move(pending_remote),
          execution_context->GetTaskRunner(TaskType::kInternalDefault));

      // TODO(crbug.com/1273291): Build the graph in the WebNN Service.
      resolver->Resolve(this);
      return;
    }
  }
}

}  // namespace blink
