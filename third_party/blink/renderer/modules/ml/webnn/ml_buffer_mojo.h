// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_MOJO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_MOJO_H_

#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class MLBufferDescriptor;

// The `Mojo` in the class name means this buffer is backed by a service running
// outside of Blink.
class MODULES_EXPORT MLBufferMojo final : public MLBuffer {
 public:
  // Create and build a concrete MLBufferMojo object.
  // Bind `WebNNBuffer` mojo interface to create `WebNNBuffer` message pipe if
  // needed.
  static MLBuffer* Create(ScopedMLTrace scoped_trace,
                          ScriptState* script_state,
                          MLContext* ml_context,
                          const MLBufferDescriptor* descriptor,
                          ExceptionState& exception_state);

  MLBufferMojo(ExecutionContext* execution_context,
               MLContext* context,
               const MLBufferDescriptor* descriptor);
  ~MLBufferMojo() override;

  void Trace(Visitor* visitor) const override;

  const base::UnguessableToken& handle() const { return webnn_handle_; }

  bool is_bound() const { return remote_buffer_.is_bound(); }

 protected:
  void DestroyImpl() override;

 private:
  void ReadBufferImpl(ScriptPromiseResolver<DOMArrayBuffer>* resolver) override;

  void WriteBufferImpl(base::span<const uint8_t> src_data,
                       ExceptionState& exception_state) override;

  // The callback of reading from `WebNNBuffer` by calling hardware accelerated
  // OS machine learning APIs.
  void OnDidReadBuffer(ScriptPromiseResolver<DOMArrayBuffer>* resolver,
                       webnn::mojom::blink::ReadBufferResultPtr result);

  // Identifies this `WebNNBuffer` mojo instance in the service process.
  const base::UnguessableToken webnn_handle_;

  // The `WebNNBuffer` is a buffer that can be used by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNBuffer> remote_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_MOJO_H_
