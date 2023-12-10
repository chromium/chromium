// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error_mojo.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

namespace {

namespace blink_mojom = webnn::mojom::blink;

base::expected<blink_mojom::GraphInfoPtr, String> BuildWebNNGraphInfo(
    const MLNamedOperands& named_outputs) {
  // The `GraphInfo` represents an entire information of WebNN graph.
  auto graph_info = blink_mojom::GraphInfo::New();
  // The id used to identify operand on the server side. Each operation
  // generates an output operand that will be inserted in a hash map with the
  // MLOperand and id, incrementing the id by one.
  uint64_t operand_id = 0;
  HeapHashMap<Member<const MLOperand>, uint64_t> operand_to_id_map;
  for (const auto& [name, operand] : named_outputs) {
    // Create `mojo::Operand` for output operands of graph with the name.
    auto output_operand =
        mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get());
    output_operand->name = name;
    operand_id++;
    graph_info->id_to_operand_map.insert(operand_id, std::move(output_operand));
    graph_info->output_operands.push_back(operand_id);
    operand_to_id_map.insert(operand, operand_id);
  }

  HeapVector<Member<const MLOperator>>* topologically_sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);
  // Visit the operators in topological order. For each operator,
  // 1, Create `mojo::Operand` for its input and output operands if needed.
  // 2, Create `mojo::Operator` with the id of input and output operands.
  for (const auto& current_operator : *topologically_sorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }
      switch (operand->Kind()) {
        case MLOperand::OperandKind::kInput: {
          // Create `mojo::Operand` for the input MLOperand.
          operand_id++;
          graph_info->id_to_operand_map.insert(
              operand_id,
              mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
          //  Build the array of input operands for this graph with the id.
          graph_info->input_operands.push_back(operand_id);
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case MLOperand::OperandKind::kConstant: {
          // Convert `mojo::Operand` for constant operand.
          operand_id++;
          graph_info->id_to_operand_map.insert(
              operand_id,
              mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
          //  Build the map of constant operands for this graph with the id.
          const auto* array_buffer_view = operand->ArrayBufferView();
          CHECK(array_buffer_view);
          CHECK(!array_buffer_view->IsDetached());
          graph_info->constant_id_to_buffer_map.insert(
              operand_id, base::make_span(static_cast<const uint8_t*>(
                                              array_buffer_view->BaseAddress()),
                                          array_buffer_view->byteLength()));
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case MLOperand::OperandKind::kOutput:
          // Because the operators are visited in topological order, if this
          // operand is an intermediate operand, it should already be defined as
          // an output operand of the dependent operator.
          NOTREACHED_NORETURN();
      }
    }

    for (const auto& operand : current_operator->Outputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }
      // Because the graph's output operands are already converted before, this
      // operand should be an intermediate operand that connects with two
      // operators. Create `mojo::Operand` for this operand.
      operand_id++;
      graph_info->id_to_operand_map.insert(
          operand_id, mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
      operand_to_id_map.insert(operand, operand_id);
    }

    // Create `mojo::Operation` with the id of the input and output operands.
    auto mojo_operation =
        ConvertToMojoOperation(operand_to_id_map, current_operator.Get());
    if (!mojo_operation.has_value()) {
      // Return here if the operator is not implemented.
      return base::unexpected(mojo_operation.error());
    }
    graph_info->operations.emplace_back(std::move(mojo_operation.value()));
  }

  return graph_info;
}

}  // namespace

// static
void MLGraphMojo::ValidateAndBuildAsync(MLContextMojo* context,
                                        const MLNamedOperands& named_outputs,
                                        ScriptPromiseResolver* resolver) {
  auto* graph =
      MakeGarbageCollected<MLGraphMojo>(resolver->GetScriptState(), context);
  graph->BuildAsync(named_outputs, resolver);
}

// static
MLGraph* MLGraphMojo::ValidateAndBuildSync(ScriptState* script_state,
                                           MLContextMojo* context,
                                           const MLNamedOperands& named_outputs,
                                           ExceptionState& exception_state) {
  auto* graph = MakeGarbageCollected<MLGraphMojo>(script_state, context);
  return graph->BuildSync(script_state, named_outputs, exception_state);
}

MLGraphMojo::MLGraphMojo(ScriptState* script_state, MLContextMojo* context)
    : MLGraph(context),
      ml_context_mojo_(context),
      remote_graph_(ExecutionContext::From(script_state)) {}

MLGraphMojo::~MLGraphMojo() = default;

void MLGraphMojo::Trace(Visitor* visitor) const {
  visitor->Trace(remote_graph_);
  visitor->Trace(ml_context_mojo_);
  MLGraph::Trace(visitor);
}

void MLGraphMojo::BuildAsyncImpl(const MLNamedOperands& outputs,
                                 ScriptPromiseResolver* resolver) {
  auto graph_info = BuildWebNNGraphInfo(outputs);
  if (!graph_info.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Failed to build graph: " + graph_info.error()));
    return;
  }
  // Create `WebNNGraph` message pipe with `WebNNContext` mojo interface.
  ml_context_mojo_->CreateWebNNGraph(
      std::move(graph_info.value()),
      WTF::BindOnce(&MLGraphMojo::OnCreateWebNNGraph, WrapPersistent(this),
                    WrapPersistent(resolver)));
}

MLGraph* MLGraphMojo::BuildSyncImpl(ScriptState* script_state,
                                    const MLNamedOperands& outputs,
                                    ExceptionState& exception_state) {
  // Ensures that this sync method is only called from worker threads.
  CHECK(!IsMainThread());
  auto graph_info = BuildWebNNGraphInfo(outputs);
  if (!graph_info.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Failed to build graph: " + graph_info.error());
    return nullptr;
  }

  blink_mojom::CreateGraphResultPtr result;
  if (!ml_context_mojo_->CreateWebNNGraphSync(std::move(graph_info.value()),
                                              &result)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to build graph.");
  }
  if (result->is_error()) {
    const auto& create_graph_error = result->get_error();
    exception_state.ThrowDOMException(
        ConvertWebNNErrorCodeToDOMExceptionCode(create_graph_error->error_code),
        create_graph_error->error_message);
    return nullptr;
  }

  auto* execution_context = ExecutionContext::From(script_state);
  remote_graph_.Bind(
      std::move(result->get_graph_remote()),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));
  return this;
}

void MLGraphMojo::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                   const MLNamedArrayBufferViews& outputs,
                                   ScriptPromiseResolver* resolver,
                                   ExceptionState& exception_state) {
  // TransferNamedArrayBufferViews deteches input and output array buffers, so
  // JavaScript can't modify them during Compute().
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

  // The inputs were already verified in the base class so we can fill the
  // buffer directly with the input tensors.
  HashMap<String, mojo_base::BigBuffer> name_to_buffer_map;
  for (const auto& [name, input_info] : *inputs_info) {
    name_to_buffer_map.insert(
        name,
        base::make_span(static_cast<const uint8_t*>(input_info.contents.Data()),
                        input_info.contents.DataLength()));
  }
  remote_graph_->Compute(
      std::move(name_to_buffer_map),
      WTF::BindOnce(&MLGraphMojo::OnDidCompute, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(inputs_info),
                    std::move(outputs_info)));
}

void MLGraphMojo::OnDidCompute(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>> inputs_info,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
        outputs_info,
    blink_mojom::ComputeResult mojo_result,
    const absl::optional<HashMap<String, mojo_base::BigBuffer>> mojo_outputs) {
  if (mojo_result != blink_mojom::ComputeResult::kOk ||
      !mojo_outputs.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        "Failed to obtain the computation result."));
    return;
  }
  for (const auto& [output_name, output_view_info] : *outputs_info) {
    // The verification before computing ensures the `ml_outputs` match graph's
    // expectation, so we only need to verify the result `mojo_outputs` from
    // WebNN Service here.
    auto output_buffer_iter = mojo_outputs->find(output_name);
    if (output_buffer_iter == mojo_outputs->end()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          "There is an unknown output tensor in the computation result: " +
              output_name));
      return;
    }
    const auto output_byte_length = output_view_info.contents.DataLength();
    if (output_buffer_iter->value.size() != output_byte_length) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "The output tensor size does not match graph's expectation: " +
              output_name));
      return;
    }
    memcpy(output_view_info.contents.Data(), output_buffer_iter->value.data(),
           output_byte_length);
  }
  auto* result = MLComputeResult::Create();
  result->setInputs(*CreateNamedArrayBufferViews(std::move(inputs_info)));
  result->setOutputs(*CreateNamedArrayBufferViews(std::move(outputs_info)));
  resolver->Resolve(result);
}

void MLGraphMojo::ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                                  const MLNamedArrayBufferViews& outputs,
                                  ExceptionState& exception_state) {
  CHECK(!IsMainThread());
  HashMap<String, mojo_base::BigBuffer> input_name_to_buffer_map;
  // Since the JavaScrip calling thread will be blocked, we don't need to detach
  // the input and output arrays.
  for (const auto& [name, array_buffer_view] : inputs) {
    input_name_to_buffer_map.insert(
        name, base::make_span(
                  static_cast<const uint8_t*>(array_buffer_view->BaseAddress()),
                  array_buffer_view->byteLength()));
  }
  blink_mojom::ComputeResult mojo_result;
  absl::optional<HashMap<String, mojo_base::BigBuffer>> mojo_outputs;
  bool call_result = remote_graph_->Compute(std::move(input_name_to_buffer_map),
                                            &mojo_result, &mojo_outputs);
  if (!call_result || mojo_result != blink_mojom::ComputeResult::kOk ||
      !mojo_outputs.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Failed to obtain the computation result.");
    return;
  }
  for (const auto& [output_name, output_buffer_view] : outputs) {
    // The verification before computing ensures the `ml_outputs` match graph's
    // expectation, so we only need to verify the result `mojo_outputs` from
    // WebNN Service here.
    auto output_buffer_iter = mojo_outputs->find(output_name);
    if (output_buffer_iter == mojo_outputs->end()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          "There is an unknown output tensor in the computation result: " +
              output_name);
      return;
    }
    const auto output_byte_length = output_buffer_view->byteLength();
    if (output_buffer_iter->value.size() != output_byte_length) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kUnknownError,
          "The output tensor size does not match graph's expectation: " +
              output_name);
      return;
    }
    memcpy(output_buffer_view->BaseAddress(), output_buffer_iter->value.data(),
           output_byte_length);
  }
}

void MLGraphMojo::OnCreateWebNNGraph(ScriptPromiseResolver* resolver,
                                     blink_mojom::CreateGraphResultPtr result) {
  // Handle error message and throw exception.
  if (result->is_error()) {
    const auto& create_graph_error = result->get_error();
    resolver->Reject(MakeGarbageCollected<DOMException>(
        ConvertWebNNErrorCodeToDOMExceptionCode(create_graph_error->error_code),
        create_graph_error->error_message));
    return;
  }

  auto* script_state = resolver->GetScriptState();
  auto* execution_context = ExecutionContext::From(script_state);
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(result->get_graph_remote()),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));

  resolver->Resolve(this);
}

}  // namespace blink
