// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include <cinttypes>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

#define THROW_AND_RETURN_IF_ERROR(func, msg)                      \
  RETURN_IF_ERROR(func, [&exception_state](const String& error) { \
    exception_state.ThrowTypeError(msg + error);                  \
    return;                                                       \
  });

#define THROW_AND_RETURN_EMPTY_PROMISE_IF_ERROR(func, msg)        \
  RETURN_IF_ERROR(func, [&exception_state](const String& error) { \
    exception_state.ThrowTypeError(msg + error);                  \
    return ScriptPromise<MLComputeResult>();                      \
  });

base::expected<void, String> ValidateNamedArrayBufferViews(
    const MLNamedArrayBufferViews& named_array_buffer_views,
    const MLGraph::NamedOperandDescriptors& expected_named_descriptors) {
  if (named_array_buffer_views.size() !=
      base::checked_cast<wtf_size_t>(expected_named_descriptors.size())) {
    return base::unexpected(String::Format(
        "The number (%u) of the array buffer views doesn't match the "
        "expectation (%u).",
        named_array_buffer_views.size(), expected_named_descriptors.size()));
  }
  for (const auto& named_array_buffer_view : named_array_buffer_views) {
    const auto& [name, array_buffer_view] = named_array_buffer_view;
    if (!expected_named_descriptors.Contains(name)) {
      return base::unexpected(String::Format(
          "The name \"%s\" isn't part of the graph.", name.Utf8().c_str()));
    }
    if (array_buffer_view->IsDetached()) {
      return base::unexpected(
          String::Format("The array buffer view with name \"%s\" is detached.",
                         name.Utf8().c_str()));
    }
    const auto& info = expected_named_descriptors.at(name);
    if (array_buffer_view->GetType() !=
        GetArrayBufferViewType(info->data_type())) {
      return base::unexpected(String::Format(
          "The type (%s) of the array buffer view with name \"%s\" doesn't "
          "match the expected operand data type (%s).",
          array_buffer_view->TypeName(), name.Utf8().c_str(),
          V8MLOperandDataType(ToBlinkDataType(info->data_type())).AsCStr()));
    }
    if (array_buffer_view->byteLength() != info->PackedByteLength()) {
      return base::unexpected(String::Format(
          "The byte length (%zu) of the array buffer view with name \"%s\" "
          "doesn't match the expected byte length (%zu).",
          array_buffer_view->byteLength(), name.Utf8().c_str(),
          info->PackedByteLength()));
    }
  }
  return base::ok();
}

base::expected<void, String> ValidateNamedMLTensors(
    const MLContext* context,
    const MLNamedTensors& named_tensors,
    const MLGraph::NamedOperandDescriptors& expected_named_descriptors) {
  if (named_tensors.size() !=
      base::checked_cast<wtf_size_t>(expected_named_descriptors.size())) {
    return base::unexpected(String::Format(
        "The number (%u) of MLTensor(s) doesn't match the "
        "expectation (%u).",
        named_tensors.size(), expected_named_descriptors.size()));
  }
  for (const auto& [name, tensor] : named_tensors) {
    if (!expected_named_descriptors.Contains(name)) {
      return base::unexpected(String::Format(
          "The name \"%s\" isn't part of the graph.", name.Utf8().c_str()));
    }
    const auto& info = expected_named_descriptors.at(name);
    if (tensor->DataType() != info->data_type()) {
      return base::unexpected(String::Format(
          "The data type \"%s\""
          ", of the MLTensor with name \"%s\" "
          "doesn't match the expected data type (%s).",
          tensor->dataType().AsCStr(), name.Utf8().c_str(),
          V8MLOperandDataType(ToBlinkDataType(info->data_type())).AsCStr()));
    }
    if (tensor->Shape() != info->shape()) {
      return base::unexpected(
          String::Format("The shape of the MLTensor with name \"%s\" "
                         "doesn't match the expected shape.",
                         name.Utf8().c_str()));
    }
    if (tensor->context() != context) {
      return base::unexpected(String::Format(
          "The context of MLGraph doesn't match the context of the MLTensor "
          "with name \"%s\".",
          name.Utf8().c_str()));
    }
  }
  return base::ok();
}

base::expected<void, String> ValidateMLTensorUsage(
    const MLNamedTensors& named_inputs,
    const MLNamedTensors& named_outputs) {
  // Validate that output tensors are unique.
  HeapHashSet<Member<MLTensor>> output_tensors;
  for (const auto& named_output : named_outputs) {
    output_tensors.insert(named_output.second);
  }

  if (output_tensors.size() != named_outputs.size()) {
    return base::unexpected(
        "The same MLTensor cannot be used more than once as output.");
  }

  // Validate tensors used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_tensors.Contains(named_input.second)) {
      return base::unexpected(
          "The same MLTensor cannot be used as input and output.");
    }
  }
  return base::ok();
}

}  // namespace

MLGraph::MLGraph(ExecutionContext* execution_context,
                 MLContext* context,
                 mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraph>
                     pending_graph_remote,
                 NamedOperandDescriptors input_constraints,
                 NamedOperandDescriptors output_constraints,
                 base::PassKey<MLGraphBuilder> /*pass_key*/)
    : input_constraints_(std::move(input_constraints)),
      output_constraints_(std::move(output_constraints)),
      ml_context_(context),
      remote_graph_(execution_context) {
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(pending_graph_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_graph_.set_disconnect_handler(
      WTF::BindOnce(&MLGraph::OnConnectionError, WrapWeakPersistent(this)));
}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_graph_);
  visitor->Trace(pending_resolvers_);
  ScriptWrappable::Trace(visitor);
}

void MLGraph::destroy() {
  if (remote_graph_.is_bound()) {
    OnConnectionError();
  }
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetInputConstraints() const {
  return input_constraints_;
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetOutputConstraints() const {
  return output_constraints_;
}

ScriptPromise<MLComputeResult> MLGraph::Compute(
    ScopedMLTrace scoped_trace,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Validate the MLNamedArrayBufferViews.
  THROW_AND_RETURN_EMPTY_PROMISE_IF_ERROR(
      ValidateNamedArrayBufferViews(inputs, input_constraints_),
      "Invalid inputs: ");
  THROW_AND_RETURN_EMPTY_PROMISE_IF_ERROR(
      ValidateNamedArrayBufferViews(outputs, output_constraints_),
      "Invalid outputs: ");

  // Remote graph gets automatically unbound when the execution context
  // destructs.
  if (!remote_graph_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Graph has been destroyed or context is lost.");
    return EmptyPromise();
  }

  HashMap<String, mojo_base::BigBuffer> name_to_buffer_map;
  for (const auto& [name, array_buffer_view] : inputs) {
    name_to_buffer_map.insert(
        name, mojo_base::BigBuffer(array_buffer_view->ByteSpan()));
  }

  // TransferNamedArrayBufferViews deteches input and output array buffers, so
  // JavaScript can't modify them during Compute().
  auto inputs_info = TransferNamedArrayBufferViews(script_state->GetIsolate(),
                                                   inputs, exception_state);
  if (!inputs_info) {
    return EmptyPromise();
  }
  auto outputs_info = TransferNamedArrayBufferViews(script_state->GetIsolate(),
                                                    outputs, exception_state);
  if (!outputs_info) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLComputeResult>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  remote_graph_->Compute(
      std::move(name_to_buffer_map),
      WTF::BindOnce(&MLGraph::DidCompute, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    std::move(inputs_info), std::move(outputs_info)));

  return resolver->Promise();
}

void MLGraph::Dispatch(ScopedMLTrace scoped_trace,
                       const MLNamedTensors& inputs,
                       const MLNamedTensors& outputs,
                       ExceptionState& exception_state) {
  // Validate the MLNamedTensors.
  THROW_AND_RETURN_IF_ERROR(
      ValidateNamedMLTensors(Context(), inputs, input_constraints_),
      "Invalid inputs: ");
  THROW_AND_RETURN_IF_ERROR(
      ValidateNamedMLTensors(Context(), outputs, output_constraints_),
      "Invalid outputs: ");
  THROW_AND_RETURN_IF_ERROR(ValidateMLTensorUsage(inputs, outputs),
                            "Invalid dispatch: ");

  // Remote graph gets automatically unbound when the execution context
  // destructs.
  if (!remote_graph_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Graph has been destroyed or context is lost.");
    return;
  }

  // The inputs and outputs were already verified in the base class so we can
  // pass the tensor directly with the input and output tensors.
  HashMap<String, blink::WebNNTensorToken> mojo_inputs;
  for (const auto& [name, input_tensor] : inputs) {
    if (!input_tensor->IsValid()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid input tensor state");
      return;
    }

    mojo_inputs.insert(name, input_tensor->handle());
  }

  HashMap<String, blink::WebNNTensorToken> mojo_outputs;
  for (const auto& [name, output_tensor] : outputs) {
    if (!output_tensor->IsValid()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid output tensor state");
      return;
    }

    mojo_outputs.insert(name, output_tensor->handle());
  }

  remote_graph_->Dispatch(std::move(mojo_inputs), std::move(mojo_outputs));
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
  pending_resolvers_.erase(resolver);

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

void MLGraph::OnConnectionError() {
  remote_graph_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Graph has been destroyed or context is lost.");
  }
  pending_resolvers_.clear();
}

}  // namespace blink
