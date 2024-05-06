// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ML;
class MLBuffer;
class MLBufferDescriptor;
class MLComputeResult;
class MLContextOptions;

class MODULES_EXPORT MLContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Resolves `resolver` with a newly created MLContext. The caller must call
  // `Promise()` on `resolver` before calling this method.
  static void ValidateAndCreate(ScriptPromiseResolver<MLContext>* resolver,
                                MLContextOptions* options,
                                ML* ml);

  // The constructor shouldn't be called directly. The callers should use the
  // ValidateAndCreate() method instead.
  MLContext(const V8MLDevicePreference device_preference,
            const V8MLDeviceType device_type,
            const V8MLPowerPreference power_preference,
            const V8MLModelFormat model_format,
            const unsigned int num_threads,
            ML* ml);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDevicePreference GetDevicePreference() const;
  V8MLDeviceType GetDeviceType() const;
  V8MLPowerPreference GetPowerPreference() const;
  V8MLModelFormat GetModelFormat() const;
  unsigned int GetNumThreads() const;
  void LogConsoleWarning(const String& message);

  ML* GetML();

  void Trace(Visitor* visitor) const override;

  // IDL interface:
  ScriptPromise<MLComputeResult> compute(ScriptState* script_state,
                                         MLGraph* graph,
                                         const MLNamedArrayBufferViews& inputs,
                                         const MLNamedArrayBufferViews& outputs,
                                         ExceptionState& exception_state);

  MLBuffer* createBuffer(ScriptState* script_state,
                         const MLBufferDescriptor* descriptor,
                         ExceptionState& exception_state);

  // Writes data specified by array buffer view from offset in elements.
  void writeBuffer(ScriptState* script_state,
                   MLBuffer* dst_buffer,
                   const MaybeShared<DOMArrayBufferView>& src_data,
                   uint64_t src_element_offset,
                   ExceptionState& exception_state);

  // Writes data specified by array buffer view from offset and size in
  // elements.
  void writeBuffer(ScriptState* script_state,
                   MLBuffer* dst_buffer,
                   const MaybeShared<DOMArrayBufferView>& src_data,
                   uint64_t src_element_offset,
                   uint64_t src_element_count,
                   ExceptionState& exception_state);

  // Writes array buffer data from offset in bytes.
  void writeBuffer(ScriptState* script_state,
                   MLBuffer* dst_buffer,
                   const DOMArrayBufferBase* src_data,
                   uint64_t src_byte_offset,
                   ExceptionState& exception_state);

  // Writes array buffer data from offset and size in bytes.
  void writeBuffer(ScriptState* script_state,
                   MLBuffer* dst_buffer,
                   const DOMArrayBufferBase* src_data,
                   uint64_t src_byte_offset,
                   uint64_t src_byte_size,
                   ExceptionState& exception_state);

  ScriptPromise<DOMArrayBuffer> readBuffer(ScriptState* script_state,
                                           MLBuffer* src_buffer,
                                           ExceptionState& exception_state);

  void dispatch(ScriptState* script_state,
                MLGraph* graph,
                const MLNamedBuffers& inputs,
                const MLNamedBuffers& outputs,
                ExceptionState& exception_state);

  // Creates a platform-specific compute graph described by `graph_info`.
  void CreateWebNNGraph(
      webnn::mojom::blink::GraphInfoPtr graph_info,
      webnn::mojom::blink::WebNNContext::CreateGraphCallback callback);

  // Creates platform specific buffer described by `buffer_info`.
  void CreateWebNNBuffer(mojo::PendingAssociatedReceiver<
                             webnn::mojom::blink::WebNNBuffer> receiver,
                         webnn::mojom::blink::BufferInfoPtr buffer_info,
                         const base::UnguessableToken& buffer_handle);

 private:
  // The callback of creating `WebNNContext` mojo interface from WebNN Service.
  // Return `CreateContextResult::kNotSupported` on non-supported input
  // configuration.
  void OnCreateWebNNContext(ScopedMLTrace scoped_trace,
                            ScriptPromiseResolver<MLContext>* resolver,
                            webnn::mojom::blink::CreateContextResultPtr result);

  // Validate and write ArrayBuffer data to hardware accelerated OS
  // machine learning buffers in the WebNN Service.
  // `src_data` is the source span of the array buffer data.
  // `src_element_offset` is the start of the data to write from in the span.
  // `src_element_count` is optional to denote when the entire span will be
  // written.
  void WriteWebNNBuffer(ScriptState* script_state,
                        MLBuffer* dst_buffer,
                        base::span<const uint8_t> src_data,
                        uint64_t src_element_offset,
                        unsigned src_data_type_size_bytes,
                        std::optional<uint64_t> src_element_count,
                        ExceptionState& exception_state);

  V8MLDevicePreference device_preference_;
  V8MLDeviceType device_type_;
  V8MLPowerPreference power_preference_;
  V8MLModelFormat model_format_;
  unsigned int num_threads_;

  Member<ML> ml_;

  // The `WebNNContext` is a initialized context that can be used by the
  // hardware accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> remote_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
