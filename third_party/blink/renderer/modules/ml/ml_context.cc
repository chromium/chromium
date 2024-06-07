// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "base/notreached.h"
#include "components/ml/webnn/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

webnn::mojom::blink::CreateContextOptions::Device ConvertBlinkDeviceTypeToMojo(
    const V8MLDeviceType& device_type_blink) {
  switch (device_type_blink.AsEnum()) {
    case V8MLDeviceType::Enum::kCpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kCpu;
    case V8MLDeviceType::Enum::kGpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kGpu;
    case V8MLDeviceType::Enum::kNpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kNpu;
  }
}

webnn::mojom::blink::CreateContextOptions::PowerPreference
ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kAuto:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kDefault;
    case V8MLPowerPreference::Enum::kLowPower:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kLowPower;
    case V8MLPowerPreference::Enum::kHighPerformance:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kHighPerformance;
  }
}

}  // namespace

// static
void MLContext::ValidateAndCreate(ScriptPromiseResolver<MLContext>* resolver,
                                  MLContextOptions* options,
                                  ML* ml) {
  ScopedMLTrace scoped_trace("MLContext::ValidateAndCreate");
  auto* context = MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->deviceType(),
      options->powerPreference(), options->modelFormat(), options->numThreads(),
      ml);

  if (base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    auto options_mojo = webnn::mojom::blink::CreateContextOptions::New(
        ConvertBlinkDeviceTypeToMojo(options->deviceType()),
        ConvertBlinkPowerPreferenceToMojo(options->powerPreference()),
        options->numThreads());

    ml->RecordPendingResolver(resolver);
    ml->CreateWebNNContext(
        std::move(options_mojo),
        WTF::BindOnce(&MLContext::OnCreateWebNNContext, WrapPersistent(context),
                      std::move(scoped_trace), WrapPersistent(resolver)));
    return;
  }

  // TODO: crbug.com/325612086 - Remove this fallback.
  resolver->Resolve(context);
}

MLContext::MLContext(const V8MLDevicePreference device_preference,
                     const V8MLDeviceType device_type,
                     const V8MLPowerPreference power_preference,
                     const V8MLModelFormat model_format,
                     const unsigned int num_threads,
                     ML* ml)
    : device_preference_(device_preference),
      device_type_(device_type),
      power_preference_(power_preference),
      model_format_(model_format),
      num_threads_(num_threads),
      ml_(ml),
      remote_context_(ml->GetExecutionContext()) {}

MLContext::~MLContext() = default;

V8MLDevicePreference MLContext::GetDevicePreference() const {
  return device_preference_;
}

V8MLDeviceType MLContext::GetDeviceType() const {
  return device_type_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

V8MLModelFormat MLContext::GetModelFormat() const {
  return model_format_;
}

unsigned int MLContext::GetNumThreads() const {
  return num_threads_;
}

void MLContext::LogConsoleWarning(const String& message) {
  auto* execution_context = ml_->GetExecutionContext();
  if (!execution_context) {
    return;
  }
  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

ML* MLContext::GetML() {
  return ml_.Get();
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  visitor->Trace(remote_context_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLComputeResult> MLContext::compute(
    ScriptState* script_state,
    MLGraph* graph,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::compute");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (graph->Context() != this) {
    exception_state.ThrowTypeError(
        "The graph isn't built within this context.");
    return EmptyPromise();
  }

  return graph->Compute(std::move(scoped_trace), inputs, outputs, script_state,
                        exception_state);
}

void MLContext::CreateWebNNGraph(
    webnn::mojom::blink::GraphInfoPtr graph_info,
    webnn::mojom::blink::WebNNContext::CreateGraphCallback callback) {
  if (!remote_context_.is_bound()) {
    std::move(callback).Run(webnn::mojom::blink::CreateGraphResult::NewError(
        webnn::mojom::blink::Error::New(
            webnn::mojom::blink::Error::Code::kUnknownError,
            "Invalid script state.")));
    return;
  }

  remote_context_->CreateGraph(std::move(graph_info),
                               WTF::BindOnce(std::move(callback)));
}

void MLContext::OnCreateWebNNContext(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<MLContext>* resolver,
    webnn::mojom::blink::CreateContextResultPtr result) {
  ml_->RemovePendingResolver(resolver);
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state) {
    return;
  }

  if (result->is_error()) {
    const auto& create_context_error = result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_context_error->code),
        create_context_error->message);
    return;
  }

  auto success = std::move(result->get_success());
  remote_context_.Bind(std::move(success->context_remote),
                       ExecutionContext::From(script_state)
                           ->GetTaskRunner(TaskType::kMachineLearning));
  properties_ = std::move(success->context_properties);

  resolver->Resolve(this);
}

void MLContext::CreateWebNNBuffer(
    mojo::PendingAssociatedReceiver<webnn::mojom::blink::WebNNBuffer> receiver,
    webnn::mojom::blink::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_context_.is_bound()) {
    return;
  }

  // Use `WebNNContext` to create `WebNNBuffer` message pipe.
  remote_context_->CreateBuffer(std::move(receiver), std::move(buffer_info),
                                buffer_handle);
}

MLBuffer* MLContext::createBuffer(ScriptState* script_state,
                                  const MLBufferDescriptor* descriptor,
                                  ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::createBuffer");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

  if (base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    return MLBuffer::Create(std::move(scoped_trace),
                            ExecutionContext::From(script_state), this,
                            descriptor, exception_state);
  }

  // TODO: crbug.com/325612086 - Remove this fallback.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

void MLContext::writeBuffer(
    ScriptState* script_state,
    MLBuffer* dst_buffer,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(),
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeBuffer(
    ScriptState* script_state,
    MLBuffer* dst_buffer,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    uint64_t src_element_count,
    ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(), src_element_count,
                   exception_state);
}

void MLContext::writeBuffer(ScriptState* script_state,
                            MLBuffer* dst_buffer,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeBuffer(ScriptState* script_state,
                            MLBuffer* dst_buffer,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            uint64_t src_byte_size,
                            ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/src_byte_size, exception_state);
}

ScriptPromise<DOMArrayBuffer> MLContext::readBuffer(
    ScriptState* script_state,
    MLBuffer* src_buffer,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_buffer->context() != this) {
    exception_state.ThrowTypeError(
        "The source buffer wasn't created with this context.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (device_type_ == V8MLDeviceType::Enum::kGpu) {
    src_buffer->ReadBufferImpl(resolver);
    return promise;
  }

  resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                   "Not implemented");

  return promise;
}

void MLContext::WriteWebNNBuffer(ScriptState* script_state,
                                 MLBuffer* dst_buffer,
                                 base::span<const uint8_t> src_data,
                                 uint64_t src_element_offset,
                                 unsigned src_data_type_size_bytes,
                                 std::optional<uint64_t> src_element_count,
                                 ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (dst_buffer->context() != this) {
    exception_state.ThrowTypeError(
        "The destination buffer wasn't created with this context.");
    return;
  }

  const size_t src_data_byte_length = src_data.size();
  if (src_element_offset > src_data_byte_length / src_data_type_size_bytes) {
    exception_state.ThrowTypeError(
        "Data offset is too large: srcOffset exceeded byte length of srcData.");
    return;
  }

  uint64_t src_byte_offset;
  if (!base::CheckMul(src_element_offset, src_data_type_size_bytes)
           .AssignIfValid(&src_byte_offset)) {
    exception_state.ThrowTypeError(
        "Data offset is too large: srcOffset will overflow.");
    return;
  }

  uint64_t max_write_size_bytes;
  if (!base::CheckSub(src_data_byte_length, src_byte_offset)
           .AssignIfValid(&max_write_size_bytes)) {
    exception_state.ThrowTypeError(
        "Number of bytes to write is too large: offset exceeds byte length.");
    return;
  }

  uint64_t write_byte_size = max_write_size_bytes;
  if (src_element_count.has_value()) {
    if (src_element_count.value() >
        max_write_size_bytes / src_data_type_size_bytes) {
      exception_state.ThrowTypeError(
          "Number of bytes to write is too large: number of elements will "
          "overflow.");
      return;
    }

    write_byte_size = src_element_count.value() * src_data_type_size_bytes;
  }

  if (write_byte_size > dst_buffer->size()) {
    exception_state.ThrowTypeError(
        "Number of bytes to write is too large: write size exceeded buffer "
        "size.");
    return;
  }

  // Write size and offset needs to be cast to size_t.
  base::CheckedNumeric<size_t> checked_write_byte_size(write_byte_size);
  if (!checked_write_byte_size.IsValid()) {
    exception_state.ThrowRangeError("Number of bytes to write is too large");
    return;
  }

  base::CheckedNumeric<size_t> checked_src_byte_offset(src_byte_offset);
  if (!checked_src_byte_offset.IsValid()) {
    exception_state.ThrowRangeError("Offset to write is too large");
    return;
  }

  if (device_type_ == V8MLDeviceType::Enum::kGpu) {
    dst_buffer->WriteBufferImpl(
        src_data.subspan(checked_src_byte_offset.ValueOrDie(),
                         checked_write_byte_size.ValueOrDie()),
        exception_state);
    return;
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
}

void MLContext::dispatch(ScriptState* script_state,
                         MLGraph* graph,
                         const MLNamedBuffers& inputs,
                         const MLNamedBuffers& outputs,
                         ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::dispatch");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (graph->Context() != this) {
    exception_state.ThrowTypeError(
        "The graph isn't built within this context.");
    return;
  }

  if (device_type_ == V8MLDeviceType::Enum::kGpu) {
    return graph->Dispatch(std::move(scoped_trace), inputs, outputs,
                           exception_state);
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
}

}  // namespace blink
