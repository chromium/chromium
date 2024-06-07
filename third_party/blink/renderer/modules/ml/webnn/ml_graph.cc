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
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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

}  // namespace

MLGraph::MLGraph(ExecutionContext* execution_context,
                 MLContext* context,
                 mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraph>
                     pending_graph_remote,
                 HashMap<String, ResourceInfo> input_resources_info,
                 HashMap<String, ResourceInfo> output_resources_info,
                 base::PassKey<MLGraphBuilder> /*pass_key*/)
    : input_resources_info_(std::move(input_resources_info)),
      output_resources_info_(std::move(output_resources_info)),
      ml_context_(context),
      remote_graph_(execution_context) {
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(pending_graph_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_graph_);
  ScriptWrappable::Trace(visitor);
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetInputResourcesInfo()
    const {
  return input_resources_info_;
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetOutputResourcesInfo()
    const {
  return output_resources_info_;
}

ScriptPromise<MLComputeResult> MLGraph::Compute(
    ScopedMLTrace scoped_trace,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ScriptState* script_state,
    ExceptionState& exception_state) {
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

}  // namespace blink
