// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_

#include "base/types/expected.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MLBufferDescriptor;
class MLContext;

class MODULES_EXPORT MLBuffer : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MLBuffer* Create(ScopedMLTrace scoped_trace,
                          ExecutionContext* execution_context,
                          MLContext* ml_context,
                          const MLBufferDescriptor* descriptor,
                          ExceptionState& exception_state);

  // The constructor shouldn't be called directly. The callers should use the
  // `Create()` method instead.
  explicit MLBuffer(ExecutionContext* execution_context,
                    MLContext* context,
                    webnn::OperandDescriptor descriptor);
  MLBuffer(const MLBuffer&) = delete;
  MLBuffer& operator=(const MLBuffer&) = delete;

  ~MLBuffer() override;

  void Trace(Visitor* visitor) const override;

  // ml_buffer.idl
  V8MLOperandDataType dataType() const;
  Vector<uint32_t> shape() const;
  void destroy();

  // Convenience methods for accessing native types, which avoid a copy
  // compared to using the corresponding methods which return blink types.
  const webnn::OperandDescriptor& Descriptor() const;
  webnn::OperandDataType DataType() const;
  const std::vector<uint32_t>& Shape() const;

  uint64_t PackedByteLength() const;

  const base::UnguessableToken& handle() const { return webnn_handle_; }

  const MLContext* context() const { return ml_context_.Get(); }

  bool IsValid() const { return remote_buffer_.is_bound(); }

  // Read data from the MLBuffer. The resolver should be resolved with a copy of
  // the buffer data. Otherwise, the resolver should be rejected accordingly.
  // The caller must call `Promise()` on `resolver` before calling this method.
  void ReadBufferImpl(ScriptPromiseResolver<DOMArrayBuffer>* resolver);

  // Write data to the MLBuffer. If write was successful, the data will be
  // stored in the MLBuffer.
  void WriteBufferImpl(base::span<const uint8_t> src_data,
                       ExceptionState& exception_state);

 private:
  // The callback of reading from `WebNNBuffer` by calling hardware accelerated
  // OS machine learning APIs.
  void OnDidReadBuffer(ScriptPromiseResolver<DOMArrayBuffer>* resolver,
                       webnn::mojom::blink::ReadBufferResultPtr result);

  webnn::mojom::blink::BufferInfoPtr GetMojoBufferInfo() const;

  Member<MLContext> ml_context_;

  // Represents a valid MLBufferDescriptor.
  const webnn::OperandDescriptor descriptor_;

  // Identifies this `WebNNBuffer` mojo instance in the service process.
  const base::UnguessableToken webnn_handle_;

  // The `WebNNBuffer` is a buffer that can be used by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNBuffer> remote_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
