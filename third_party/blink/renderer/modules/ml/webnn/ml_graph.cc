// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include <cinttypes>

#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
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

#define REJECT_AND_RETURN_IF_ERROR(func, msg)              \
  RETURN_IF_ERROR(func, [&resolver](const String& error) { \
    resolver->RejectWithTypeError(msg + error);            \
    return;                                                \
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

}  // namespace

MLGraph::MLGraph(MLContext* context) : ml_context_(context) {}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
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

void MLGraph::Compute(ScopedMLTrace scoped_trace,
                      const MLNamedArrayBufferViews& inputs,
                      const MLNamedArrayBufferViews& outputs,
                      ScriptPromiseResolver<MLComputeResult>* resolver,
                      ExceptionState& exception_state) {
  // The MLGraph object should be initialized before computing.
  DCHECK(resources_info_initialized_);

  // Validate the MLNamedArrayBufferViews.
  REJECT_AND_RETURN_IF_ERROR(
      ValidateNamedArrayBufferViews(inputs, input_resources_info_),
      "Invalid inputs: ");
  REJECT_AND_RETURN_IF_ERROR(
      ValidateNamedArrayBufferViews(outputs, output_resources_info_),
      "Invalid outputs: ");

  // Call ComputeImpl() implemented by an MLGraph backend.
  ComputeImpl(std::move(scoped_trace), inputs, outputs, resolver,
              exception_state);
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

  // Call DispatchImpl() implemented by an MLGraph backend.
  DispatchImpl(std::move(scoped_trace), inputs, outputs, exception_state);
}

void MLGraph::Build(ScopedMLTrace scoped_trace,
                    const MLNamedOperands& named_outputs,
                    ScriptPromiseResolver<MLGraph>* resolver) {
  REJECT_AND_RETURN_IF_ERROR(ValidateAndInitializeResourcesInfo(named_outputs),
                             "");
  BuildImpl(std::move(scoped_trace), named_outputs, resolver);
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
          // `BuildImpl()`. A constant operand may carry a detached
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

}  // namespace blink
