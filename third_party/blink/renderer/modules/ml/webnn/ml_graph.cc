// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

namespace {

bool ValidateNamedArrayBufferViews(
    const MLNamedArrayBufferViews& named_array_buffer_views,
    const HashMap<String, MLGraph::ResourceInfo>& resources_info,
    String& error_message) {
  if (named_array_buffer_views.size() !=
      base::checked_cast<wtf_size_t>(resources_info.size())) {
    error_message = String::Format(
        "The number (%u) of the array buffer views doesn't match the "
        "expectation (%u).",
        named_array_buffer_views.size(),
        base::checked_cast<wtf_size_t>(resources_info.size()));
    return false;
  }
  for (const auto& named_array_buffer_view : named_array_buffer_views) {
    const auto& [name, array_buffer_view] = named_array_buffer_view;
    if (!resources_info.Contains(name)) {
      error_message = String::Format("The name \"%s\" isn't part of the graph.",
                                     name.Utf8().c_str());
      return false;
    }
    if (array_buffer_view->IsDetached()) {
      error_message =
          String::Format("The array buffer view with name \"%s\" is detached.",
                         name.Utf8().c_str());
      return false;
    }
    const auto& info = resources_info.at(name);
    if (array_buffer_view->GetType() != GetArrayBufferViewType(info.type)) {
      error_message = String::Format(
          "The type (%s) of the array buffer view with name \"%s\" doesn't "
          "match the expected operand type (%s).",
          array_buffer_view->TypeName(), name.Utf8().c_str(),
          V8MLOperandType(info.type).AsCStr());
      return false;
    }
    if (array_buffer_view->byteLength() != info.byte_length) {
      error_message = String::Format(
          "The byte length (%zu) of the array buffer view with name \"%s\" "
          "doesn't match the expected byte length (%zu).",
          array_buffer_view->byteLength(), name.Utf8().c_str(),
          info.byte_length);
      return false;
    }
  }
  return true;
}

DOMArrayBufferView* TransferArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source_view,
    ExceptionState& exception_state) {
  // A detached ArrayBufferView should be caught by
  // `ValidateNamedArrayBufferViews()`.
  DCHECK(!source_view->IsDetached());

  // Avoid transferring a non-detachable ArrayBuffer.
  // `DOMArrayBuffer::Transfer()` would make a copy if the ArrayBuffer is not
  // detachable. This behavior doesn't follow the algorithm to transfer an
  // ArrayBuffer of WebIDL spec:
  // https://webidl.spec.whatwg.org/#arraybuffer-transfer
  if (!source_view->buffer()->IsDetachable(isolate)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The ArrayBuffer is not detachable.");
    return nullptr;
  }

  // Get the offset and length of the source view before transferring it.
  const auto offset = source_view->byteOffset();
  const auto length = source_view->byteLength() / source_view->TypeSize();

  ArrayBufferContents target_contents;
  // The following `DOMArrayBuffer::Transfer()` call would fail if the
  // detach key of the ArrayBuffer is not `undefined`.
  if (!source_view->buffer()->Transfer(isolate, target_contents,
                                       exception_state)) {
    return nullptr;
  }

  auto* target_buffer = DOMArrayBuffer::Create(std::move(target_contents));

  // Align with the ArrayBufferView types supported by WebNN MLOperandType:
  // https://www.w3.org/TR/webnn/#appendices-mloperandtype-arraybufferview-compatibility
  DOMArrayBufferView* target_view = nullptr;
  switch (source_view->GetType()) {
    case DOMArrayBufferView::kTypeFloat32:
      // Float32Array is used for MLOperandType::float32.
      target_view = DOMFloat32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      target_view = DOMUint16Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeInt32:
      // Int32Array is used for MLOperandType::int32.
      target_view = DOMInt32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint32:
      // Uint32Array is used for MLOperandType::uint32.
      target_view = DOMUint32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeInt8:
      // Int8Array is used for MLOperandType::int8.
      target_view = DOMInt8Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint8:
      // Uint8Array is used for MLOperandType::uint8.
      target_view = DOMUint8Array::Create(target_buffer, offset, length);
      break;
    default:
      // Other ArrayBufferView types should not pass the
      // `ValidateNamedArrayBufferViews()` and reach here.
      NOTREACHED();
  }
  return target_view;
}

// Implement the MLNamedArrayBufferViews transfer algorithm of WebNN spec:
// https://www.w3.org/TR/webnn/#mlnamedarraybufferviews-transfer
//
// If it fails to transfer an ArrayBufferView of the MLNamedArrayBufferViews,
// the current implementation leaves the already-transferred views detached, the
// failing one and remaining others unchanged.
//
// TODO(crbug.com/1273291): Revisit the error handling once the WebNN spec issue
// is resolved: https://github.com/webmachinelearning/webnn/issues/351
MLNamedArrayBufferViews* TransferNamedArrayBufferViews(
    v8::Isolate* isolate,
    const MLNamedArrayBufferViews& source_views,
    ExceptionState& exception_state) {
  auto* target_views = MakeGarbageCollected<MLNamedArrayBufferViews>();
  for (const auto& [name, source_view] : source_views) {
    NotShared<DOMArrayBufferView> target_view(
        TransferArrayBufferView(isolate, source_view, exception_state));
    if (!target_view.Get()) {
      return nullptr;
    }
    target_views->push_back(std::make_pair(name, target_view));
  }
  return target_views;
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

void MLGraph::ComputeAsync(const MLNamedArrayBufferViews& inputs,
                           const MLNamedArrayBufferViews& outputs,
                           ScriptPromiseResolver* resolver,
                           ExceptionState& exception_state) {
  // The MLGraph object should be initialized before computing.
  DCHECK(resources_info_initialized_);

  // Validate the MLNamedArrayBufferViews.
  String error_message;
  if (!ValidateNamedArrayBufferViews(inputs, input_resources_info_,
                                     error_message)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "Invalid inputs: " + error_message));
    return;
  }
  if (!ValidateNamedArrayBufferViews(outputs, output_resources_info_,
                                     error_message)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "Invalid outputs: " + error_message));
    return;
  }
  // Transfer the MLNamedArrayBufferViews.
  auto* transferred_inputs = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), inputs, exception_state);
  if (!transferred_inputs) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid inputs: " + exception_state.Message()));
    return;
  }
  auto* transferred_outputs = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), outputs, exception_state);
  if (!transferred_outputs) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid outputs: " + exception_state.Message()));
    return;
  }

  // Call ComputeAsyncImpl() implemented by an MLGraph backend.
  ComputeAsyncImpl(*transferred_inputs, *transferred_outputs, resolver);
}

void MLGraph::ComputeSync(const MLNamedArrayBufferViews& inputs,
                          const MLNamedArrayBufferViews& outputs,
                          ExceptionState& exception_state) {
  // The MLGraph object should be initialized before computing.
  DCHECK(resources_info_initialized_);

  // Validate the input and output MLNamedArrayBufferViews.
  String error_message;
  if (!ValidateNamedArrayBufferViews(inputs, input_resources_info_,
                                     error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Invalid inputs: " + error_message);
    return;
  }
  if (!ValidateNamedArrayBufferViews(outputs, output_resources_info_,
                                     error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Invalid outputs: " + error_message);
    return;
  }

  // Call ComputeSyncImpl() implemented by an MLGraph backend.
  ComputeSyncImpl(inputs, outputs, exception_state);
}

void MLGraph::BuildAsync(const MLNamedOperands& named_outputs,
                         ScriptPromiseResolver* resolver) {
  String error_message;
  if (!ValidateAndInitializeResourcesInfo(named_outputs, error_message)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, error_message));
    return;
  }
  BuildAsyncImpl(named_outputs, resolver);
}

MLGraph* MLGraph::BuildSync(const MLNamedOperands& named_outputs,
                            ExceptionState& exception_state) {
  String error_message;
  if (!ValidateAndInitializeResourcesInfo(named_outputs, error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  return BuildSyncImpl(named_outputs, exception_state);
}

bool MLGraph::ValidateAndInitializeResourcesInfo(
    const MLNamedOperands& named_outputs,
    String& error_message) {
  DCHECK(!resources_info_initialized_);

  // The outputs should not be empty.
  if (named_outputs.empty()) {
    error_message = "At least one output needs to be provided.";
    return false;
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
    if (operand->Kind() != MLOperand::OperandKind::kOutput) {
      error_message = String::Format(
          "The operand with name \"%s\" is not an output operand.",
          name.Utf8().c_str());
      return false;
    }
    // Setup resource info for this output operand.
    output_resources_info_.insert(
        name, ResourceInfo({.type = operand->Type(),
                            .byte_length = operand->ByteLength()}));
    // Mark its dependent operator is visited.
    visited_operators.insert(operand->Operator());
    // Enqueue its dependent operator.
    operators_queue.push_back(operand->Operator());
  }

  while (operators_queue.size() > 0) {
    // If the queue is not empty, dequeue an operator from the queue.
    const auto current_operator = operators_queue.TakeFirst();
    // Enumerate the current operator's input operands.
    for (const auto& operand : current_operator->Inputs()) {
      switch (operand->Kind()) {
        case MLOperand::OperandKind::kOutput:
          DCHECK(operand->Operator());
          // If the operand is an output operand and its dependent operator is
          // not visited, mark the dependent operator is visited and enqueue
          // it.
          if (!visited_operators.Contains(operand->Operator())) {
            visited_operators.insert(operand->Operator());
            operators_queue.push_back(operand->Operator());
          }
          break;
        case MLOperand::OperandKind::kInput:
          // If the operand is an input operand, validate whether its name is
          // unique.
          if (input_resources_info_.Contains(operand->Name())) {
            error_message =
                String::Format("The input name \"%s\" is duplicated.",
                               operand->Name().Utf8().c_str());
            return false;
          }
          // Setup resource info for this input operand.
          input_resources_info_.insert(
              operand->Name(),
              ResourceInfo({.type = operand->Type(),
                            .byte_length = operand->ByteLength()}));
          break;
        case MLOperand::OperandKind::kConstant:
          // If the operand is a constant operand, there is no check needed.
          break;
      }
    }
  }
  resources_info_initialized_ = true;
  return true;
}

const MLContext* MLGraph::Context() const {
  return ml_context_.Get();
}

}  // namespace blink
