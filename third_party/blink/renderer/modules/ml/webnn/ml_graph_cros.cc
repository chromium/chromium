// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_cros.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

flatbuffers::DetachedBuffer* g_flatbuffer_for_testing = nullptr;

}  // namespace

// static
void MLGraphCrOS::ValidateAndBuildAsync(MLContext* ml_context,
                                        const MLNamedOperands& named_outputs,
                                        ScriptPromiseResolver* resolver) {
  auto* script_state = resolver->GetScriptState();
  auto* execution_context = ExecutionContext::From(script_state);
  auto* graph =
      MakeGarbageCollected<MLGraphCrOS>(execution_context, ml_context);
  graph->BuildAsync(named_outputs, resolver);
}

MLGraphCrOS::MLGraphCrOS(ExecutionContext* execution_context,
                         MLContext* ml_context)
    : MLGraph(ml_context), remote_model_(execution_context) {}

MLGraphCrOS::~MLGraphCrOS() = default;

void MLGraphCrOS::Trace(Visitor* visitor) const {
  visitor->Trace(remote_model_);
  MLGraph::Trace(visitor);
}

void MLGraphCrOS::BuildAsyncImpl(const MLNamedOperands& outputs,
                                 ScriptPromiseResolver* resolver) {
  DOMArrayBuffer* buffer = nullptr;
  if (g_flatbuffer_for_testing) {
    buffer = DOMArrayBuffer::Create(g_flatbuffer_for_testing->data(),
                                    g_flatbuffer_for_testing->size());
  }
  // TODO(crbug.com/1273291): Convert WebNN graph to flatbuffer in the `buffer`.
  auto* script_state = resolver->GetScriptState();
  auto* execution_context = ExecutionContext::From(script_state);
  auto* ml_model_loader = ml_context_->GetModelLoaderForWebNN(script_state);
  ml_model_loader->Load(
      script_state, buffer,
      WTF::BindOnce(&MLGraphCrOS::OnRemoteModelLoad, WrapPersistent(this),
                    WrapPersistent(execution_context),
                    WrapPersistent(resolver)));
}

void MLGraphCrOS::OnRemoteModelLoad(
    ExecutionContext* execution_context,
    ScriptPromiseResolver* resolver,
    ml::model_loader::mojom::blink::LoadModelResult result,
    mojo::PendingRemote<ml::model_loader::mojom::blink::Model> pending_remote,
    ml::model_loader::mojom::blink::ModelInfoPtr tensor_info) {
  if (result != ml::model_loader::mojom::blink::LoadModelResult::kOk) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Unknown error."));
    return;
  }
  remote_model_.Bind(
      std::move(pending_remote),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));

  // Stores input tensor information of loaded model to verify the input
  // data by user including name and byte length.
  input_tensor_name_to_info_ = std::move(tensor_info->input_tensor_info);
  // Stores output tensor information of loaded model to verify the output
  // data returned from `MLService` after computing.
  output_tensor_name_to_info_ = std::move(tensor_info->output_tensor_info);

  resolver->Resolve(this);
}

const HashMap<String, ml::model_loader::mojom::blink::TensorInfoPtr>&
MLGraphCrOS::GetInputTensorInfoMapForTesting() const {
  return input_tensor_name_to_info_;
}

const HashMap<String, ml::model_loader::mojom::blink::TensorInfoPtr>&
MLGraphCrOS::GetOutputTensorInfoMapForTesting() const {
  return output_tensor_name_to_info_;
}

// static
void MLGraphCrOS::SetFlatbufferForTesting(
    flatbuffers::DetachedBuffer* flatbuffer) {
  g_flatbuffer_for_testing = flatbuffer;
}

MLGraph* MLGraphCrOS::BuildSyncImpl(const MLNamedOperands& named_outputs,
                                    ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support sync build that is only exposed to
  // dedicated worker.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented.");
  return nullptr;
}

void MLGraphCrOS::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                   const MLNamedArrayBufferViews& outputs,
                                   ScriptPromiseResolver* resolver,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support async compute.
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Not implemented."));
}

void MLGraphCrOS::ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                                  const MLNamedArrayBufferViews& outputs,
                                  ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Support sync compute that is only exposed to
  // dedicated worker.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented.");
}

}  // namespace blink
