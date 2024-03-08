// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer_mojo.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_buffer_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error_mojo.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

// static
MLBuffer* MLBufferMojo::Create(ScopedMLTrace scoped_trace,
                               ScriptState* script_state,
                               MLContext* ml_context,
                               const MLBufferDescriptor* descriptor,
                               ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Execution context is detached.");
    return nullptr;
  }

  // ml_context is required.
  CHECK(ml_context);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  auto* buffer = MakeGarbageCollected<MLBufferMojo>(execution_context,
                                                    ml_context, descriptor);
  scoped_trace.AddStep("MLBufferMojo::Create");

  // Create `WebNNBuffer` message pipe with `WebNNContext` mojo interface.
  ml_context->CreateWebNNBuffer(
      buffer->remote_buffer_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kInternalDefault)),
      blink_mojom::BufferInfo::New(descriptor->size()), buffer->handle());

  return buffer;
}

MLBufferMojo::MLBufferMojo(ExecutionContext* execution_context,
                           MLContext* ml_context,
                           const MLBufferDescriptor* descriptor)
    : MLBuffer(ml_context, descriptor->size()),
      webnn_handle_(base::UnguessableToken::Create()),
      remote_buffer_(execution_context) {}

MLBufferMojo::~MLBufferMojo() = default;

void MLBufferMojo::Trace(Visitor* visitor) const {
  visitor->Trace(remote_buffer_);
  MLBuffer::Trace(visitor);
}

void MLBufferMojo::DestroyImpl() {
  // Calling reset on a bound remote will disconnect or destroy the buffer in
  // the service. The remote buffer must remain unbound after calling destroy()
  // because it is valid to call destroy() multiple times.
  remote_buffer_.reset();
}

}  // namespace blink
