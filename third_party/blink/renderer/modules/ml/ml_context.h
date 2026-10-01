// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace blink {

class ExecutionContext;
class MLTensor;
class MLTensorDescriptor;
class MLContextLostInfo;
class MLOpSupportLimits;
class GPUBuffer;
class GPUDevice;

class MODULES_EXPORT MLContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLContext(
      ExecutionContext* execution_context,
      const V8MLDeviceType device_type,
      const V8MLPowerPreference power_preference,
      webnn::mojom::blink::CreateContextSuccessPtr create_context_success);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDeviceType GetDeviceType() const;
  V8MLPowerPreference GetPowerPreference() const;

  const webnn::ContextProperties& GetProperties() { return properties_; }

  void Trace(Visitor* visitor) const override;

  const blink::WebNNContextToken& handle() const { return webnn_handle_; }

  // IDL interface:
  ScriptPromise<MLContextLostInfo> lost(ScriptState* script_state);

  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  ScriptPromise<MLTensor> createTensor(ScriptState* script_state,
                                       const MLTensorDescriptor* descriptor,
                                       ExceptionState& exception_state);

  ScriptPromise<MLTensor> createExportableTensor(
      ScriptState* script_state,
      const MLTensorDescriptor* descriptor,
      GPUDevice* device,
      ExceptionState& exception_state);

  ScriptPromise<MLTensor> createConstantTensor(
      ScriptState* script_state,
      const MLOperandDescriptor* descriptor,
      const V8AllowSharedBufferSource* src_data,
      ExceptionState& exception_state);

  void writeTensor(ScriptState* script_state,
                   MLTensor* dst_tensor,
                   const V8AllowSharedBufferSource* src_data,
                   ExceptionState& exception_state);

  ScriptPromise<DOMArrayBuffer> readTensor(ScriptState* script_state,
                                           MLTensor* src_tensor,
                                           ExceptionState& exception_state);

  ScriptPromise<IDLUndefined> readTensor(ScriptState* script_state,
                                         MLTensor* src_tensor,
                                         V8AllowSharedBufferSource* dst_data,
                                         ExceptionState& exception_state);

  void dispatch(ScriptState* script_state,
                MLGraph* graph,
                const MLNamedTensors& inputs,
                const MLNamedTensors& outputs,
                ExceptionState& exception_state);

  GPUBuffer* exportToGPU(ScriptState* script_state,
                         MLTensor* tensor,
                         ExceptionState& exception_state);

  MLGraphBuilder* CreateWebNNGraphBuilder(ScriptState* script_state,
                                          ExceptionState& exception_state);

  gpu::SyncToken GenerateVerifiedReleaseToken();

  const MLOpSupportLimits* opSupportLimits(ScriptState* script_state);

  void OnGraphCreated(MLGraph* graph);

  // Sends DestroyGraph through the context pipe to ensure ordering with
  // Dispatch/ReadTensor/WriteTensor. Called by MLGraph::destroy().
  void DestroyGraph(const blink::WebNNGraphToken& graph_token);

  const mojo::ScopedDataPipeProducerHandle& write_tensor_producer() const {
    return write_tensor_producer_;
  }

  const mojo::ScopedDataPipeConsumerHandle& read_tensor_consumer() const {
    return read_tensor_consumer_;
  }

 private:
  using LostProperty = ScriptPromiseProperty<MLContextLostInfo, IDLUndefined>;

  // Close the `context_remote_` and `compiler_context_remote_` pipes
  // because the entire context has been lost.
  void OnLost(uint32_t custom_reason, const std::string& description);

  // Called when the compiler context remote disconnects. Does not eagerly
  // reconnect; the next CreateWebNNGraphBuilder() triggers reconnection.
  void OnCompilerContextDisconnected();

  void DidCreateWebNNTensor(webnn::ScopedTrace scoped_trace,
                            ScriptPromiseResolver<blink::MLTensor>* resolver,
                            webnn::OperandDescriptor validated_descriptor,
                            webnn::MLTensorUsage usage,
                            scoped_refptr<gpu::ClientSharedImage> shared_image,
                            GPUDevice* gpu_device,
                            webnn::mojom::blink::CreateTensorResultPtr result);

  V8MLDeviceType device_type_;
  V8MLPowerPreference power_preference_;

  Member<LostProperty> lost_property_;

  // The `WebNNContext` is a initialized context that can be used by the
  // hardware accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> context_remote_;

  // Optional remote to the Compiler process for graph building.
  // Set when the backend offloads compilation (e.g., ORT).
  HeapMojoRemote<webnn::mojom::blink::WebNNCompilerContext>
      compiler_context_remote_;

  // Whether the backend routes graph building through a separate Compiler
  // process. Not a renderer-side choice: set at context creation from whether
  // the GPU returned a `compiler_context_remote`.
  // Cached so that after the compiler context remote disconnects (e.g. after a
  // Compiler process crash or idle shutdown), CreateWebNNGraphBuilder()
  // reconnects to the Compiler process instead of building the graph through
  // `context_remote_`.
  bool backend_uses_compiler_process_ = false;

  webnn::ContextProperties properties_;

  mojo::ScopedDataPipeProducerHandle write_tensor_producer_;
  mojo::ScopedDataPipeConsumerHandle read_tensor_consumer_;

  // Identifies this `WebNNContext` mojo instance in the service process.
  const blink::WebNNContextToken webnn_handle_;

  // Keep a set of unresolved `ScriptPromiseResolver`s which will be
  // rejected when the Mojo pipe is unexpectedly disconnected.
  HeapHashSet<Member<ScriptPromiseResolver<MLTensor>>> pending_resolvers_;

  HeapHashSet<WeakMember<MLGraph>> graphs_;
  HeapHashSet<WeakMember<MLGraphBuilder>> graph_builders_;
  HeapHashSet<WeakMember<MLTensor>> tensors_;

  const gpu::CommandBufferId command_buffer_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  uint64_t last_sync_token_release_id_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
