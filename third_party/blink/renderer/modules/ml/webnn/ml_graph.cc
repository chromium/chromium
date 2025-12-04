// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include "base/task/single_thread_task_runner.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

#define THROW_AND_RETURN_IF_ERROR(func, msg)                      \
  RETURN_IF_ERROR(func, [&exception_state](const String& error) { \
    exception_state.ThrowTypeError(StrCat({msg, error}));         \
    return;                                                       \
  });

template <typename T>
void AppendVectorOfNumbers(const std::vector<T>& vector,
                           StringBuilder& builder) {
  builder.AppendRange(vector, ", ");
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
      StringBuilder message;
      message.Append("The shape [");
      AppendVectorOfNumbers(tensor->Shape(), message);
      message.Append("], of the MLTensor with name \"");
      message.Append(name);
      message.Append("\" doesn't match the expected shape: [");
      AppendVectorOfNumbers(info->shape(), message);
      message.Append("]");
      return base::unexpected(message.ToString());
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
                 Vector<V8MLDeviceType> devices,
                 base::PassKey<MLGraphBuilder> /*pass_key*/)
    : input_constraints_(std::move(input_constraints)),
      output_constraints_(std::move(output_constraints)),
      ml_context_(context),
      remote_graph_(execution_context),
      devices_(std::move(devices)) {
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(pending_graph_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_graph_.set_disconnect_handler(
      BindOnce(&MLGraph::OnConnectionError, WrapWeakPersistent(this)));
}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_graph_);
  ScriptWrappable::Trace(visitor);
}

void MLGraph::destroy() {
  if (remote_graph_.is_bound()) {
    OnConnectionError();
  }
}

Vector<V8MLDeviceType> MLGraph::devices() const {
  return devices_;
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetInputConstraints() const {
  return input_constraints_;
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetOutputConstraints() const {
  return output_constraints_;
}

void MLGraph::Dispatch(webnn::ScopedTrace scoped_trace,
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

    if (input_tensor->Usage().Has(webnn::MLTensorUsageFlags::kGraphConstant)) {
      exception_state.ThrowTypeError("Invalid input tensor usage");
      return;
    }

    if (input_tensor->is_exported_to_webgpu()) {
      exception_state.ThrowTypeError(
          "Input tensor has been exported to WebGPU");
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

    if (output_tensor->Usage().Has(webnn::MLTensorUsageFlags::kGraphConstant)) {
      exception_state.ThrowTypeError("Invalid output tensor usage");
      return;
    }

    if (output_tensor->is_exported_to_webgpu()) {
      exception_state.ThrowTypeError(
          "Output tensor has been exported to WebGPU");
      return;
    }

    mojo_outputs.insert(name, output_tensor->handle());
  }

  remote_graph_->Dispatch(std::move(mojo_inputs), std::move(mojo_outputs));
}

const MLContext* MLGraph::Context() const {
  return ml_context_.Get();
}

void MLGraph::OnConnectionError() {
  remote_graph_.reset();
}

}  // namespace blink
