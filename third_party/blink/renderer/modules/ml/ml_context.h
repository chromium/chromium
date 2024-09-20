// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExecutionContext;
class MLTensor;
class MLTensorDescriptor;
class MLComputeResult;
class MLContextLostInfo;
class MLOpSupportLimits;

class MODULES_EXPORT MLContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLContext(
      ExecutionContext* execution_context,
      const V8MLDeviceType device_type,
      const V8MLPowerPreference power_preference,
      const unsigned int num_threads,
      webnn::mojom::blink::CreateContextSuccessPtr create_context_success);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDeviceType GetDeviceType() const;
  V8MLPowerPreference GetPowerPreference() const;
  unsigned int GetNumThreads() const;

  const webnn::ContextProperties& GetProperties() { return properties_; }

  void Trace(Visitor* visitor) const override;

  const blink::WebNNContextToken& handle() const { return webnn_handle_; }

  // IDL interface:
  ScriptPromise<MLContextLostInfo> lost(ScriptState* script_state);

  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  ScriptPromise<MLComputeResult> compute(ScriptState* script_state,
                                         MLGraph* graph,
                                         const MLNamedArrayBufferViews& inputs,
                                         const MLNamedArrayBufferViews& outputs,
                                         ExceptionState& exception_state);

  ScriptPromise<MLTensor> createTensor(ScriptState* script_state,
                                       const MLTensorDescriptor* descriptor,
                                       ExceptionState& exception_state);

  // Writes data specified by array buffer view from offset in elements.
  void writeTensor(ScriptState* script_state,
                   MLTensor* dst_tensor,
                   const MaybeShared<DOMArrayBufferView>& src_data,
                   uint64_t src_element_offset,
                   ExceptionState& exception_state);

  // Writes data specified by array buffer view from offset and size in
  // elements.
  void writeTensor(ScriptState* script_state,
                   MLTensor* dst_tensor,
                   const MaybeShared<DOMArrayBufferView>& src_data,
                   uint64_t src_element_offset,
                   uint64_t src_element_count,
                   ExceptionState& exception_state);

  // Writes array tensor data from offset in bytes.
  void writeTensor(ScriptState* script_state,
                   MLTensor* dst_tensor,
                   const DOMArrayBufferBase* src_data,
                   uint64_t src_byte_offset,
                   ExceptionState& exception_state);

  // Writes array tensor data from offset and size in bytes.
  void writeTensor(ScriptState* script_state,
                   MLTensor* dst_tensor,
                   const DOMArrayBufferBase* src_data,
                   uint64_t src_byte_offset,
                   uint64_t src_byte_size,
                   ExceptionState& exception_state);

  ScriptPromise<DOMArrayBuffer> readTensor(ScriptState* script_state,
                                           MLTensor* src_tensor,
                                           ExceptionState& exception_state);

  ScriptPromise<IDLUndefined> readTensor(ScriptState* script_state,
                                         MLTensor* src_tensor,
                                         DOMArrayBufferBase* dst_data,
                                         ExceptionState& exception_state);

  ScriptPromise<IDLUndefined> readTensor(
      ScriptState* script_state,
      MLTensor* src_tensor,
      MaybeShared<DOMArrayBufferView> dst_data,
      ExceptionState& exception_state);

  void dispatch(ScriptState* script_state,
                MLGraph* graph,
                const MLNamedTensors& inputs,
                const MLNamedTensors& outputs,
                ExceptionState& exception_state);

  MLGraphBuilder* CreateWebNNGraphBuilder(ScriptState* script_state,
                                          ExceptionState& exception_state);

  const MLOpSupportLimits* opSupportLimits(ScriptState* script_state);

  void OnGraphCreated(MLGraph* graph);

 private:
  using LostProperty = ScriptPromiseProperty<MLContextLostInfo, IDLUndefined>;

  // Close the `context_remote_` pipe because the context has been lost.
  void OnLost(uint32_t custom_reason, const std::string& description);

  // Validate and write ArrayBuffer data to hardware accelerated OS
  // machine learning buffers in the WebNN Service.
  // `src_data` is the source span of the array buffer data.
  // `src_element_offset` is the start of the data to write from in the span.
  // `src_element_count` is optional to denote when the entire span will be
  // written.
  void WriteWebNNTensor(ScriptState* script_state,
                        MLTensor* dst_tensor,
                        base::span<const uint8_t> src_data,
                        uint64_t src_element_offset,
                        unsigned src_data_type_size_bytes,
                        std::optional<uint64_t> src_element_count,
                        ExceptionState& exception_state);

  void DidCreateWebNNTensor(ScopedMLTrace scoped_trace,
                            ScriptPromiseResolver<blink::MLTensor>* resolver,
                            webnn::OperandDescriptor validated_descriptor,
                            webnn::MLTensorUsage usage,
                            webnn::mojom::blink::CreateTensorResultPtr result);

  V8MLDeviceType device_type_;
  V8MLPowerPreference power_preference_;
  unsigned int num_threads_;

  Member<LostProperty> lost_property_;

  // The `WebNNContext` is a initialized context that can be used by the
  // hardware accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> context_remote_;
  webnn::ContextProperties properties_;

  // Identifies this `WebNNContext` mojo instance in the service process.
  const blink::WebNNContextToken webnn_handle_;

  // Keep a set of unresolved `ScriptPromiseResolver`s which will be
  // rejected when the Mojo pipe is unexpectedly disconnected.
  HeapHashSet<Member<ScriptPromiseResolver<MLTensor>>> pending_resolvers_;

  HeapHashSet<WeakMember<MLGraph>> graphs_;
  HeapHashSet<WeakMember<MLGraphBuilder>> graph_builders_;
  HeapHashSet<WeakMember<MLTensor>> buffers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
