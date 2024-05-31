// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include <cinttypes>

#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

namespace {

#define THROW_AND_RETURN_IF_ERROR(func, msg)                      \
  RETURN_IF_ERROR(func, [&exception_state](const String& error) { \
    exception_state.ThrowTypeError(msg + error);                  \
    return;                                                       \
  });

#define THROW_AND_RETURN_TYPE_IF_ERROR(func, return_value, msg)   \
  RETURN_IF_ERROR(func, [&exception_state](const String& error) { \
    exception_state.ThrowTypeError(msg + error);                  \
    return return_value;                                          \
  });

base::expected<void, String> ValidateNamedArrayBufferViews(
    const MLNamedArrayBufferViews& named_array_buffer_views,
    const HashMap<String, MLGraph::ResourceInfo>& resources_info) {
  if (named_array_buffer_views.size() !=
      base::checked_cast<wtf_size_t>(resources_info.size())) {
    return base::unexpected(String::Format(
        "The number (%u) of the array buffer views doesn't match the "
        "expectation (%u).",
        named_array_buffer_views.size(), resources_info.size()));
  }
  for (const auto& named_array_buffer_view : named_array_buffer_views) {
    const auto& [name, array_buffer_view] = named_array_buffer_view;
    if (!resources_info.Contains(name)) {
      return base::unexpected(String::Format(
          "The name \"%s\" isn't part of the graph.", name.Utf8().c_str()));
    }
    if (array_buffer_view->IsDetached()) {
      return base::unexpected(
          String::Format("The array buffer view with name \"%s\" is detached.",
                         name.Utf8().c_str()));
    }
    const auto& info = resources_info.at(name);
    if (array_buffer_view->GetType() !=
        GetArrayBufferViewType(info.data_type)) {
      return base::unexpected(String::Format(
          "The type (%s) of the array buffer view with name \"%s\" doesn't "
          "match the expected operand data type (%s).",
          array_buffer_view->TypeName(), name.Utf8().c_str(),
          V8MLOperandDataType(info.data_type).AsCStr()));
    }
    if (array_buffer_view->byteLength() != info.byte_length) {
      return base::unexpected(String::Format(
          "The byte length (%zu) of the array buffer view with name \"%s\" "
          "doesn't match the expected byte length (%zu).",
          array_buffer_view->byteLength(), name.Utf8().c_str(),
          info.byte_length));
    }
  }
  return base::ok();
}

base::expected<void, String> ValidateNamedMLBuffers(
    const MLContext* context,
    const MLNamedBuffers& named_buffers,
    const HashMap<String, MLGraph::ResourceInfo>& resources_info) {
  if (named_buffers.size() !=
      base::checked_cast<wtf_size_t>(resources_info.size())) {
    return base::unexpected(
        String::Format("The number (%u) of MLBuffer(s) doesn't match the "
                       "expectation (%u).",
                       named_buffers.size(), resources_info.size()));
  }
  for (const auto& [name, buffer] : named_buffers) {
    if (!resources_info.Contains(name)) {
      return base::unexpected(String::Format(
          "The name \"%s\" isn't part of the graph.", name.Utf8().c_str()));
    }
    const auto& info = resources_info.at(name);
    if (buffer->size() != info.byte_length) {
      return base::unexpected(String::Format(
          "The size %" PRIu64
          ", of the MLBuffer with name \"%s\" "
          "doesn't match the expected byte length (%zu).",
          buffer->size(), name.Utf8().c_str(), info.byte_length));
    }
    if (buffer->context() != context) {
      return base::unexpected(String::Format(
          "The context of MLGraph doesn't match the context of the MLBuffer "
          "with name \"%s\".",
          name.Utf8().c_str()));
    }
  }
  return base::ok();
}

base::expected<void, String> ValidateMLBufferUsage(
    const MLNamedBuffers& named_inputs,
    const MLNamedBuffers& named_outputs) {
  // Validate that output buffers are unique.
  HeapHashSet<Member<MLBuffer>> output_buffers;
  for (const auto& named_output : named_outputs) {
    output_buffers.insert(named_output.second);
  }

  if (output_buffers.size() != named_outputs.size()) {
    return base::unexpected(
        "The same MLBuffer cannot be used more than once as output.");
  }

  // Validate buffers used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_buffers.Contains(named_input.second)) {
      return base::unexpected(
          "The same MLBuffer cannot be used as input and output.");
    }
  }
  return base::ok();
}

uint64_t NextOperandId(const webnn::mojom::blink::GraphInfo& graph_info) {
  // This count must start at 1 because 0 is a reserved element in a
  // WTF::HashMap (yes, really).
  return graph_info.id_to_operand_map.size() + 1;
}

base::expected<webnn::mojom::blink::GraphInfoPtr, String> BuildWebNNGraphInfo(
    const MLNamedOperands& named_outputs,
    const webnn::mojom::blink::ContextProperties& context_properties) {
  // The `GraphInfo` represents an entire information of WebNN graph.
  auto graph_info = webnn::mojom::blink::GraphInfo::New();

  HeapHashMap<Member<const MLOperand>, uint64_t> operand_to_id_map;
  for (const auto& [name, operand] : named_outputs) {
    // Create `mojo::Operand` for output operands of graph with the name.
    auto output_operand =
        mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get());
    output_operand->name = name;
    uint64_t operand_id = NextOperandId(*graph_info);
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
        case webnn::mojom::blink::Operand::Kind::kInput: {
          // Create `mojo::Operand` for the input MLOperand.
          uint64_t operand_id = NextOperandId(*graph_info);
          graph_info->id_to_operand_map.insert(
              operand_id,
              mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get()));
          //  Build the array of input operands for this graph with the id.
          graph_info->input_operands.push_back(operand_id);
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case webnn::mojom::blink::Operand::Kind::kConstant: {
          // Convert `mojo::Operand` for constant operand.
          uint64_t operand_id = NextOperandId(*graph_info);
          graph_info->id_to_operand_map.insert(
              operand_id,
              mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get()));
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
        case webnn::mojom::blink::Operand::Kind::kOutput:
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
      uint64_t operand_id = NextOperandId(*graph_info);
      graph_info->id_to_operand_map.insert(
          operand_id,
          mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get()));
      operand_to_id_map.insert(operand, operand_id);
    }

    // Create `mojo::Operation` with the id of the input and output operands.
    std::optional<String> error =
        SerializeMojoOperation(operand_to_id_map, context_properties,
                               current_operator.Get(), graph_info.get());
    if (error.has_value()) {
      // Return here if the operator is not implemented.
      return base::unexpected(*error);
    }
  }

  return graph_info;
}

}  // namespace

// static
void MLGraph::CreateAndBuild(ScopedMLTrace scoped_trace,
                             MLContext* context,
                             const MLNamedOperands& named_outputs,
                             ScriptPromiseResolver<MLGraph>* resolver) {
  CHECK(context);
  CHECK(resolver);

  auto* graph =
      MakeGarbageCollected<MLGraph>(resolver->GetExecutionContext(), context);
  scoped_trace.AddStep("MLGraph::CreateAndBuild");

  // TODO(crbug.com/40278771): Replace with THROW_AND_RETURN_IF_ERROR.
  RETURN_IF_ERROR(graph->ValidateAndInitializeResourcesInfo(named_outputs),
                  [&resolver](const String& error) {
                    resolver->RejectWithTypeError(error);
                    return;
                  });

  auto graph_info =
      BuildWebNNGraphInfo(named_outputs, context->GetProperties());
  if (!graph_info.has_value()) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Failed to build graph: " + graph_info.error());
    return;
  }

  context->CreateWebNNGraph(
      std::move(graph_info.value()),
      WTF::BindOnce(&MLGraph::DidCreateWebNNGraph, WrapPersistent(graph),
                    std::move(scoped_trace), WrapPersistent(resolver)));
}

MLGraph::MLGraph(ExecutionContext* execution_context, MLContext* context)
    : ml_context_(context), remote_graph_(execution_context) {}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_graph_);
  ScriptWrappable::Trace(visitor);
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetInputResourcesInfo()
    const {
  DCHECK(resources_info_initialized_);
  return input_resources_info_;
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetOutputResourcesInfo()
    const {
  DCHECK(resources_info_initialized_);
  return output_resources_info_;
}

ScriptPromise<MLComputeResult> MLGraph::Compute(
    ScopedMLTrace scoped_trace,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // The MLGraph object should be initialized before computing.
  DCHECK(resources_info_initialized_);

  // Validate the MLNamedArrayBufferViews.
  THROW_AND_RETURN_TYPE_IF_ERROR(
      ValidateNamedArrayBufferViews(inputs, input_resources_info_),
      ScriptPromise<MLComputeResult>(), "Invalid inputs: ");
  THROW_AND_RETURN_TYPE_IF_ERROR(
      ValidateNamedArrayBufferViews(outputs, output_resources_info_),
      ScriptPromise<MLComputeResult>(), "Invalid outputs: ");

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLComputeResult>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  HashMap<String, mojo_base::BigBuffer> name_to_buffer_map;
  for (const auto& [name, array_buffer_view] : inputs) {
    name_to_buffer_map.insert(
        name, mojo_base::BigBuffer(array_buffer_view->ByteSpan()));
  }

  // TransferNamedArrayBufferViews deteches input and output array buffers, so
  // JavaScript can't modify them during Compute().
  auto inputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), inputs, exception_state);
  if (!inputs_info) {
    resolver->Reject(exception_state);
    return promise;
  }
  auto outputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), outputs, exception_state);
  if (!outputs_info) {
    resolver->Reject(exception_state);
    return promise;
  }

  remote_graph_->Compute(
      std::move(name_to_buffer_map),
      WTF::BindOnce(&MLGraph::DidCompute, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    std::move(inputs_info), std::move(outputs_info)));

  return promise;
}

void MLGraph::Dispatch(ScopedMLTrace scoped_trace,
                       const MLNamedBuffers& inputs,
                       const MLNamedBuffers& outputs,
                       ExceptionState& exception_state) {
  // The MLGraph object should be initialized before dispatching.
  DCHECK(resources_info_initialized_);

  // Validate the MLNamedBuffers.
  THROW_AND_RETURN_IF_ERROR(
      ValidateNamedMLBuffers(Context(), inputs, input_resources_info_),
      "Invalid inputs: ");
  THROW_AND_RETURN_IF_ERROR(
      ValidateNamedMLBuffers(Context(), outputs, output_resources_info_),
      "Invalid outputs: ");
  THROW_AND_RETURN_IF_ERROR(ValidateMLBufferUsage(inputs, outputs),
                            "Invalid dispatch: ");

  // Remote graph gets automatically unbound when the execution context
  // destructs.
  if (!remote_graph_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid graph state");
    return;
  }

  // The inputs and outputs were already verified in the base class so we can
  // pass the buffer directly with the input and output tensors.
  HashMap<String, base::UnguessableToken> mojo_inputs;
  for (const auto& [name, input_buffer] : inputs) {
    if (!input_buffer->IsValid()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid input buffer state");
      return;
    }

    mojo_inputs.insert(name, input_buffer->handle());
  }

  HashMap<String, base::UnguessableToken> mojo_outputs;
  for (const auto& [name, output_buffer] : outputs) {
    if (!output_buffer->IsValid()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid output buffer state");
      return;
    }

    mojo_outputs.insert(name, output_buffer->handle());
  }

  remote_graph_->Dispatch(std::move(mojo_inputs), std::move(mojo_outputs));
}

base::expected<void, String> MLGraph::ValidateAndInitializeResourcesInfo(
    const MLNamedOperands& named_outputs) {
  DCHECK(!resources_info_initialized_);

  // The outputs should not be empty.
  if (named_outputs.empty()) {
    return base::unexpected("At least one output needs to be provided.");
  }

  // The queue and visited set of operators that help implement the
  // breadth-first graph traversal:
  // https://en.wikipedia.org/wiki/Breadth-first_search
  HeapDeque<Member<const MLOperator>> operators_queue;
  HeapHashSet<Member<const MLOperator>> visited_operators;

  // Validate the named outputs, setup corresponding output resource info and
  // initialize the queue and visited set with their dependent operators.
  for (const auto& output : named_outputs) {
    const auto& name = output.first;
    const auto& operand = output.second;
    // Validate whether it is an output operand.
    if (operand->Kind() != webnn::mojom::blink::Operand::Kind::kOutput) {
      return base::unexpected(String::Format(
          "The operand with name \"%s\" is not an output operand.",
          name.Utf8().c_str()));
    }
    // Setup resource info for this output operand.
    output_resources_info_.insert(
        name, ResourceInfo({.data_type = operand->DataType(),
                            .byte_length = operand->ByteLength()}));
    // Mark its dependent operator is visited.
    visited_operators.insert(operand->Operator());
    // Enqueue its dependent operator.
    operators_queue.push_back(operand->Operator());
  }

  // An input MLOperand may be used by more than one MLOperators. This set
  // ensures an input MLOperand won't be validated multiple times.
  HeapHashSet<Member<const MLOperand>> visited_input_operands;
  while (operators_queue.size() > 0) {
    // If the queue is not empty, dequeue an operator from the queue.
    const auto current_operator = operators_queue.TakeFirst();
    // Enumerate the current operator's input operands.
    for (const auto& operand : current_operator->Inputs()) {
      switch (operand->Kind()) {
        case webnn::mojom::blink::Operand::Kind::kOutput:
          DCHECK(operand->Operator());
          // If the operand is an output operand and its dependent operator is
          // not visited, mark the dependent operator is visited and enqueue
          // it.
          if (!visited_operators.Contains(operand->Operator())) {
            visited_operators.insert(operand->Operator());
            operators_queue.push_back(operand->Operator());
          }
          break;
        case webnn::mojom::blink::Operand::Kind::kInput:
          // If the operand has been validated, it doesn't need to be verified
          // multiple times.
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          // If the operand is an input operand, validate whether its name is
          // unique.
          if (input_resources_info_.Contains(operand->Name())) {
            return base::unexpected(
                String::Format("The input name \"%s\" is duplicated.",
                               operand->Name().Utf8().c_str()));
          }
          // Setup resource info for this input operand.
          input_resources_info_.insert(
              operand->Name(),
              ResourceInfo({.data_type = operand->DataType(),
                            .byte_length = operand->ByteLength()}));
          break;
        case webnn::mojom::blink::Operand::Kind::kConstant:
          // If the operand has been validated, it doesn't need to be verified
          // multiple times.
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          // If the operand is a constant operand, validate its ArrayBufferView
          // is not detached, because the backends may access its content in
          // `CreateAndBuild()`. A constant operand may carry a detached
          // ArrayBufferView if the JS code first calls
          // `MLGraphBuilder.constant()` to build a constant operand with a
          // valid ArrayBufferView, then detaches the ArrayBufferView and calls
          // `MLGraphBuilder.build()` to build the graph with this constant
          // operand.
          CHECK(operand->ArrayBufferView());
          if (operand->ArrayBufferView()->IsDetached()) {
            return base::unexpected(
                "The array buffer view of the constant operand is detached.");
          }
          break;
      }
    }
  }
  resources_info_initialized_ = true;
  return base::ok();
}

const MLContext* MLGraph::Context() const {
  return ml_context_.Get();
}

void MLGraph::DidCompute(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<MLComputeResult>* resolver,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>> inputs_info,
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
        outputs_info,
    webnn::mojom::blink::ComputeResultPtr mojo_result) {
  if (mojo_result->is_error()) {
    const auto& compute_error = mojo_result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(compute_error->code),
        compute_error->message);
    return;
  }

  const auto& mojo_outputs = mojo_result->get_named_outputs();
  auto* outputs = MakeGarbageCollected<MLNamedArrayBufferViews>();
  outputs->reserve(outputs_info->size());
  for (auto& [output_name, output_view_info] : *outputs_info) {
    // The verification before computing ensures the `ml_outputs` match graph's
    // expectation, so we only need to verify the result `mojo_outputs` from
    // WebNN Service here.
    auto output_buffer_iter = mojo_outputs.find(output_name);
    if (output_buffer_iter == mojo_outputs.end()) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          "There is an unknown output tensor in the computation result: " +
              output_name);
      return;
    }
    DOMArrayBufferView* output_view =
        CreateArrayBufferView(std::move(output_view_info));
    CHECK(output_view);
    auto output_buffer = base::make_span(output_buffer_iter->value);
    if (output_buffer.size() != output_view->byteLength()) {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "The output tensor size does not match graph's expectation: " +
              output_name);
      return;
    }
    output_view->ByteSpan().copy_from(output_buffer);
    outputs->push_back(std::make_pair(output_name, output_view));
  }
  auto* result = MLComputeResult::Create();
  result->setInputs(*CreateNamedArrayBufferViews(std::move(inputs_info)));
  result->setOutputs(*outputs);
  resolver->Resolve(result);
}

// TODO(crbug.com/325612086): Once all backends use mojo, consider refactoring
// MLGraph creation such that this logic can live in MLGraphBuilder.
void MLGraph::DidCreateWebNNGraph(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<MLGraph>* resolver,
    webnn::mojom::blink::CreateGraphResultPtr result) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state) {
    return;
  }

  // Handle error message and throw exception.
  if (result->is_error()) {
    const auto& create_graph_error = result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_graph_error->code),
        create_graph_error->message);
    return;
  }

  auto* execution_context = ExecutionContext::From(script_state);
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(result->get_graph_remote()),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));

  resolver->Resolve(this);
}

}  // namespace blink
