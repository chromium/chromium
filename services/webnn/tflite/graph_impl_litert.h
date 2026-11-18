// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/webnn/public/cpp/webgpu_context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace litert {

class ContextImplLiteRt;

// TODO(crbug.com/524317888): Replace this struct with the type defined directly
// in LiteRT headers once exported.
struct LiteRtWebGpuFlushCallback {
  void (*callback)(void* user_data) = nullptr;
  // RAW_PTR_EXCLUSION: Must match the LiteRT C callback ABI.
  RAW_PTR_EXCLUSION void* user_data = nullptr;
};

// Owns the per-`MLContext` WebGPU device/procs and C-ABI flush callback shared
// across all `GraphImplLiteRt::ComputeResources` created on that context.
// ML Drift's `Instance::SetFlushCallback` stores a single callback/`user_data`
// pair per `WGPUDevice` (keyed by device pointer with a reference count), so
// all graphs sharing a `WGPUDevice` must register the same
// `LiteRtWebGpuFlushCallback` address and keep its backing
// `base::RepeatingClosure` alive until the context and every graph using the
// device have torn down their LiteRT model and environment.
//
// Inherits from `RefCountedDeleteOnSequence` so `properties_.webgpu_flush`
// (which captures sequence-affine Dawn wire client objects) is always destroyed
// on the context's `owning_task_runner`, even if the last reference is dropped
// on an intermediate reply closure.
class SharedWebGpuResources
    : public base::RefCountedDeleteOnSequence<SharedWebGpuResources> {
 public:
  SharedWebGpuResources(
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
      WebGpuContextProperties properties);

  SharedWebGpuResources(const SharedWebGpuResources&) = delete;
  SharedWebGpuResources& operator=(const SharedWebGpuResources&) = delete;

  const WebGpuContextProperties& properties() const { return properties_; }
  const LiteRtWebGpuFlushCallback* flush_callback() const {
    return properties_.webgpu_flush.is_null() ? nullptr : &flush_callback_;
  }

 private:
  friend class base::RefCountedDeleteOnSequence<SharedWebGpuResources>;
  friend class base::DeleteHelper<SharedWebGpuResources>;

  ~SharedWebGpuResources();

  WebGpuContextProperties properties_;
  LiteRtWebGpuFlushCallback flush_callback_;
};

// GraphImplLiteRt inherits from WebNNGraphImpl to represent a LiteRT graph
// implementation. It is mainly responsible for building a LiteRT flatbuffer
// model from mojom::GraphInfo via tflite::GraphBuilderTflite, then initializing
// and executing the graph.
class GraphImplLiteRt final : public WebNNGraphImpl {
 public:
  static void CreateAndBuild(
      mojom::GraphInfoPtr graph_info,
      ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      ContextImplLiteRt& context,
      scoped_refptr<SharedWebGpuResources> webgpu_resources,
      base::File weights_file,
      mojo::PendingRemote<mojom::WeightsFileSession> session,
      WebNNContextImpl::CreateGraphImplCallback callback);

  class ComputeResources;
  GraphImplLiteRt(ComputeResourceInfo compute_resource_info,
                  std::unique_ptr<ComputeResources> compute_resources,
                  WebNNContextImpl& context,
                  std::vector<mojom::Device> devices);

  GraphImplLiteRt(const GraphImplLiteRt&) = delete;
  GraphImplLiteRt& operator=(const GraphImplLiteRt&) = delete;

 private:
  ~GraphImplLiteRt() override;

  // Builds the graph only (writes weights via session sync IPC). Returns
  // the builder `Result`.
  static base::expected<tflite::GraphBuilderTflite::Result, mojom::ErrorPtr>
  BuildGraphOnBackgroundThread(
      ContextProperties context_properties,
      mojom::Device context_device,
      mojom::GraphInfoPtr graph_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, base::flat_set<OperationId>>
          operand_to_dependent_operations,
      base::flat_map<OperandId, OperationId> operand_to_producing_operation,
      base::File weights_file,
      mojo::SharedRemote<mojom::WeightsFileSession> session);

  // Called on context sequence after the graph is built. Finalizes the session
  // if present, then dispatches compute resource creation.
  static void DidBuildGraph(
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info,
      mojom::Device context_device,
      bool is_xnnpack_enabled,
      scoped_refptr<SharedWebGpuResources> webgpu_resources,
      mojo::SharedRemote<mojom::WeightsFileSession> session,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<tflite::GraphBuilderTflite::Result, mojom::ErrorPtr>
          result);

  // Dispatches compute resource creation to the appropriate sequence
  // (context sequence for WebGPU, or thread pool for CPU/NPU).
  static void DispatchCreateComputeResources(
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info,
      mojom::Device context_device,
      bool is_xnnpack_enabled,
      scoped_refptr<SharedWebGpuResources> webgpu_resources,
      WebNNContextImpl::CreateGraphImplCallback callback,
      tflite::GraphBuilderTflite::Result build_result);

  // Create compute resources on the task runner selected by
  // DispatchCreateComputeResources().
  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  CreateComputeResources(mojom::Device context_device,
                         bool is_xnnpack_enabled,
                         scoped_refptr<SharedWebGpuResources> webgpu_resources,
                         tflite::GraphBuilderTflite::Result result);

  static void DidCreateAndBuild(
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback,
      base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
          result);

  // Execute the compiled platform graph. The inputs were validated in base
  // class so we can use them to compute directly.
  void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs)
      override;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<ComputeResources> compute_resources_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace litert
}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_IMPL_LITERT_H_
