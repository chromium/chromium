// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_

#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
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
  // Instances should only be constructed via `MLContext.createBuffer()`.
  // This method is public as required by the `MakeGarbageCollected` helper.
  //
  // `descriptor` describes the buffer data type and shape.
  // `create_buffer_success` contains the resulting handles to the created
  // buffer. which may be used to execute a context operation with respective
  // buffer.
  MLBuffer(ExecutionContext* execution_context,
           MLContext* context,
           webnn::OperandDescriptor descriptor,
           webnn::mojom::blink::CreateBufferSuccessPtr create_buffer_success,
           base::PassKey<MLContext> pass_key);
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

  const blink::WebNNBufferToken& handle() const { return webnn_handle_; }

  const MLContext* context() const { return ml_context_.Get(); }

  bool IsValid() const { return remote_buffer_.is_bound(); }

  // Read data from the MLBuffer. The resolver should be resolved with a copy of
  // the buffer data. Otherwise, the resolver should be rejected accordingly.
  ScriptPromise<DOMArrayBuffer> ReadBufferImpl(ScriptState* script_state,
                                               ExceptionState& exception_state);

  ScriptPromise<void> ReadBufferImpl(ScriptState* script_state,
                                     DOMArrayBufferBase* dst_data,
                                     ExceptionState& exception_state);

  ScriptPromise<void> ReadBufferImpl(ScriptState* script_state,
                                     DOMArrayBufferView* dst_data,
                                     ExceptionState& exception_state);

  // Write data to the MLBuffer. If write was successful, the data will be
  // stored in the MLBuffer.
  void WriteBufferImpl(base::span<const uint8_t> src_data,
                       ExceptionState& exception_state);

 private:
  // The callback of reading from `WebNNBuffer` by calling hardware accelerated
  // OS machine learning APIs.
  void OnDidReadBuffer(ScriptPromiseResolver<DOMArrayBuffer>* resolver,
                       webnn::mojom::blink::ReadBufferResultPtr result);
  void OnDidReadBufferByob(ScriptPromiseResolver<void>* resolver,
                           DOMArrayBufferBase* dst_data,
                           webnn::mojom::blink::ReadBufferResultPtr result);
  void OnDidReadBufferByobView(ScriptPromiseResolver<void>* resolver,
                               DOMArrayBufferView* dst_data,
                               webnn::mojom::blink::ReadBufferResultPtr result);

  void OnConnectionError();

  Member<MLContext> ml_context_;

  // Represents a valid MLBufferDescriptor.
  const webnn::OperandDescriptor descriptor_;

  // Identifies this `WebNNBuffer` mojo instance in the service process.
  const blink::WebNNBufferToken webnn_handle_;

  // The `WebNNBuffer` is a buffer that can be used by the hardware
  // accelerated OS machine learning API.
  HeapMojoAssociatedRemote<webnn::mojom::blink::WebNNBuffer> remote_buffer_;

  // Keep a set of unresolved `ScriptPromiseResolver`s which will be
  // rejected when the Mojo pipe is unexpectedly disconnected.
  HeapHashSet<Member<ScriptPromiseResolver<DOMArrayBuffer>>> pending_resolvers_;
  HeapHashSet<Member<ScriptPromiseResolver<void>>> pending_byob_resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
