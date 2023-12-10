// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_cros.h"

#include "base/numerics/checked_math.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_tflite_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/flatbuffers/src/include/flatbuffers/detached_buffer.h"

namespace blink {

namespace {

flatbuffers::DetachedBuffer* g_flatbuffer_for_testing = nullptr;

using ml::model_loader::mojom::blink::ComputeResult;

base::expected<bool, String> ValidateModelLoadedTensorInfo(
    const HashMap<String, ml::model_loader::mojom::blink::TensorInfoPtr>&
        model_tensor_info,
    const HashMap<String, MLGraph::ResourceInfo>& graph_resources_info) {
  if (model_tensor_info.size() != graph_resources_info.size()) {
    return base::unexpected(
        "The number of model loaded tensor info doesn't match graph's "
        "expectation.");
  }
  for (const auto& [name, mojo_tensor] : model_tensor_info) {
    if (!graph_resources_info.Contains(name)) {
      return base::unexpected(String::Format(
          "The name \"%s\" isn't part of the graph.", name.Utf8().c_str()));
    }
    if (mojo_tensor->byte_size != graph_resources_info.at(name).byte_length) {
      return base::unexpected(String::Format(
          "The byte length of the model loaded tensor info with name \"%s\" "
          "doesn't match graph's expectation.",
          name.Utf8().c_str()));
    }
  }
  return true;
}

base::expected<flatbuffers::DetachedBuffer, String> BuildTfLiteModel(
    const MLNamedOperands& named_outputs) {
  MLGraphTfLiteConverter converter;
  // Map the operand to its index of `tflite::Tensor` array which holds all
  // tensors used in the model.
  HeapHashMap<Member<const MLOperand>, int32_t> operand_to_index_map;
  for (const auto& [name, operand] : named_outputs) {
    // Serialize output operand of graph into the flat buffer.
    const auto tensor_index = converter.SerializeTensor(operand, name);
    operand_to_index_map.insert(operand, tensor_index);
  }

  const auto* toposorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);
  CHECK(toposorted_operators);
  // Visit the operators in topological order. For each operator,
  // 1. Build `tflite::Tensor` for its input and output operands if needed.
  // 2. Build `tflite::Operator` with the tensor index of inputs and outputs
  //    operand.
  for (const auto& current_operator : *toposorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand_to_index_map.Contains(operand.Get())) {
        // The tensor is already built for this operand, skip it.
        continue;
      }
      switch (operand->Kind()) {
        case MLOperand::OperandKind::kInput:
        case MLOperand::OperandKind::kConstant: {
          // Serialize tensor for input or constant operand.
          auto tensor_index = converter.SerializeTensor(operand.Get());
          operand_to_index_map.insert(operand, tensor_index);
          break;
        }
        case MLOperand::OperandKind::kOutput:
          // Because the operators are visited in topological order, if this
          // operand is an intermediate operand, it should already be defined as
          // an output operand of the dependent operator.
          NOTREACHED();
          break;
      }
    }

    for (const auto& operand : current_operator->Outputs()) {
      if (operand_to_index_map.Contains(operand.Get())) {
        // The tensor is already built for this operand, skip it.
        continue;
      }
      // Because the graph's output operands are already converted before, this
      // operand should be an intermediate operand that connects with two
      // operators.
      const auto tensor_index = converter.SerializeTensor(operand.Get());
      operand_to_index_map.insert(operand, tensor_index);
    }

    const auto serialized_result = converter.SerializeOperation(
        operand_to_index_map, current_operator.Get());
    if (!serialized_result.has_value()) {
      return base::unexpected(serialized_result.error());
    }
  }

  // Build the model in the flat buffer and return the detached Buffer.
  return converter.FinishAndTakeFlatBuffer();
}

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
  } else {
    // Convert WebNN graph to TF-Lite model in flat buffer with the schema.
    const auto flatbuffer = BuildTfLiteModel(outputs);
    if (!flatbuffer.has_value()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, flatbuffer.error()));
      return;
    }
    buffer = DOMArrayBuffer::Create(flatbuffer->data(), flatbuffer->size());
  }

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
  // Verifies the inputs from model are expected for the WebNN graph.
  auto valid_inputs = ValidateModelLoadedTensorInfo(
      tensor_info->input_tensor_info, input_resources_info_);
  if (!valid_inputs.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid inputs: " + valid_inputs.error()));
    return;
  }
  // Verifies the outputs from model are expected for the WebNN graph.
  auto valid_outputs = ValidateModelLoadedTensorInfo(
      tensor_info->output_tensor_info, output_resources_info_);
  if (!valid_outputs.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid outputs: " + valid_outputs.error()));
    return;
  }

  remote_model_.Bind(
      std::move(pending_remote),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));

  resolver->Resolve(this);
}

// static
void MLGraphCrOS::SetFlatbufferForTesting(
    flatbuffers::DetachedBuffer* flatbuffer) {
  g_flatbuffer_for_testing = flatbuffer;
}

MLGraph* MLGraphCrOS::BuildSyncImpl(ScriptState* script_state,
                                    const MLNamedOperands& named_outputs,
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
  // Transfer the `MLNamedArrayBufferViews` to `NamedArrayBufferViewsInfo` which
  // is safe to compute asynchronously.
  auto inputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), inputs, exception_state);
  if (!inputs_info) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid inputs: " + exception_state.Message()));
    return;
  }
  auto outputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), outputs, exception_state);
  if (!outputs_info) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid outputs: " + exception_state.Message()));
    return;
  }
  // The inputs were already verified in the base class. so we can fill the
  // buffer directly with the input tensors.
  HashMap<String, Vector<uint8_t>> input_mojo;
  for (const auto& [name, input_info] : *inputs_info) {
    wtf_size_t input_byte_length =
        base::checked_cast<wtf_size_t>(input_info.contents.DataLength());
    Vector<uint8_t> tensor(input_byte_length);
    memcpy(tensor.data(), input_info.contents.Data(), input_byte_length);

    input_mojo.insert(name, std::move(tensor));
  }
  remote_model_->Compute(
      std::move(input_mojo),
      WTF::BindOnce(&MLGraphCrOS::OnComputeGraph, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(inputs_info),
                    std::move(outputs_info)));
}

void MLGraphCrOS::OnComputeGraph(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>> inputs_info,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
        outputs_info,
    ComputeResult mojo_result,
    const absl::optional<HashMap<String, Vector<uint8_t>>>& mojo_outputs) {
  if (mojo_result != ComputeResult::kOk || !mojo_outputs.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        "Failed to obtain the computation result."));
    return;
  }

  for (const auto& [name, view_info] : *outputs_info) {
    // The verification before computing ensures the `ml_outputs` match graph's
    // expectation, so we only need to verify the `mojo_outputs` here.
    auto output_tensor_data = mojo_outputs.value().find(name);
    if (output_tensor_data == mojo_outputs.value().end()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          "Failed to get result for the output " + name));
      return;
    }
    if (output_tensor_data->value.size() != view_info.contents.DataLength()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "The output tensor size does not match graph's expectation: " +
              name));
      return;
    }
    memcpy(view_info.contents.Data(), output_tensor_data->value.data(),
           output_tensor_data->value.size());
  }
  auto* result = MLComputeResult::Create();
  result->setInputs(*CreateNamedArrayBufferViews(std::move(inputs_info)));
  result->setOutputs(*CreateNamedArrayBufferViews(std::move(outputs_info)));
  resolver->Resolve(result);
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
