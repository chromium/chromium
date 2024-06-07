// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_buffer_descriptor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"

namespace blink {

// static
base::expected<MLBuffer::ValidatedDescriptor, String>
MLBuffer::ValidatedDescriptor::Create(uint64_t size) {
  return ValidatedDescriptor(size);
}

MLBuffer::ValidatedDescriptor::ValidatedDescriptor(uint64_t size)
    : size_(size) {}

// static
MLBuffer* MLBuffer::Create(ScopedMLTrace scoped_trace,
                           ExecutionContext* execution_context,
                           MLContext* ml_context,
                           const MLBufferDescriptor* descriptor,
                           ExceptionState& exception_state) {
  CHECK(ml_context);
  CHECK(execution_context);

  auto validated_descriptor = ValidatedDescriptor::Create(descriptor->size());
  if (!validated_descriptor.has_value()) {
    exception_state.ThrowTypeError(validated_descriptor.error());
    return nullptr;
  }

  auto* buffer = MakeGarbageCollected<MLBuffer>(
      execution_context, ml_context, *std::move(validated_descriptor));
  scoped_trace.AddStep("MLBuffer::Create");

  // Create `WebNNBuffer` message pipe with `WebNNContext` mojo interface.
  ml_context->CreateWebNNBuffer(
      buffer->remote_buffer_.BindNewEndpointAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kMachineLearning)),
      buffer->GetMojoBufferInfo(), buffer->handle());

  return buffer;
}

MLBuffer::MLBuffer(ExecutionContext* execution_context,
                   MLContext* context,
                   ValidatedDescriptor descriptor)
    : ml_context_(context),
      descriptor_(std::move(descriptor)),
      webnn_handle_(base::UnguessableToken::Create()),
      remote_buffer_(execution_context) {}

MLBuffer::~MLBuffer() = default;

void MLBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_buffer_);
  ScriptWrappable::Trace(visitor);
}

void MLBuffer::destroy() {
  // Calling reset on a bound remote will disconnect or destroy the buffer in
  // the service. The remote buffer must remain unbound after calling destroy()
  // because it is valid to call destroy() multiple times.
  remote_buffer_.reset();
}

uint64_t MLBuffer::size() const {
  return descriptor_.size();
}

void MLBuffer::ReadBufferImpl(ScriptPromiseResolver<DOMArrayBuffer>* resolver) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Invalid buffer state");
    return;
  }

  remote_buffer_->ReadBuffer(WTF::BindOnce(&MLBuffer::OnDidReadBuffer,
                                           WrapPersistent(this),
                                           WrapPersistent(resolver)));
}

// TODO(crbug.com/40278771): Keep a set of unresolved resolvers and reject them
// if `remote_buffer_` encounters a connection error.
void MLBuffer::OnDidReadBuffer(
    ScriptPromiseResolver<DOMArrayBuffer>* resolver,
    webnn::mojom::blink::ReadBufferResultPtr result) {
  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_buffer_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_buffer_error.code),
        read_buffer_error.message);
    return;
  }
  resolver->Resolve(DOMArrayBuffer::Create(result->get_buffer().data(),
                                           result->get_buffer().size()));
}

void MLBuffer::WriteBufferImpl(base::span<const uint8_t> src_data,
                               ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid buffer state");
    return;
  }

  // Copy src data.
  remote_buffer_->WriteBuffer(src_data);
}

webnn::mojom::blink::BufferInfoPtr MLBuffer::GetMojoBufferInfo() const {
  return webnn::mojom::blink::BufferInfo::New(descriptor_.size());
}

}  // namespace blink
