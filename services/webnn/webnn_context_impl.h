// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_

#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/graph_builder_context.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_object_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
namespace tflite {
class ContextProviderTflite;
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

class WebNNContextProviderImpl;
class WebNNGraphBuilderImpl;
class WebNNTensorImpl;
class ScopedGpuSequence;

// A WebNNContextImpl owns a collection of graphs and tensors and may be bound
// to a device such as a GPU or NPU. It is created and destroyed on its
// `owning_task_runner()`. Mojo messages are dispatched on
// `task_runner()`, which is a distinct task runner but runs on the
// same thread.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextImpl
    : public WebNNObjectBase<mojom::WebNNContext,
                             blink::WebNNContextToken,
                             mojo::Receiver<mojom::WebNNContext>>,
      public GraphBuilderContext,
      public base::trace_event::MemoryDumpProvider {
 public:
  // These values are persisted to logs. Entries should not be renumbered or
  // removed and numeric values should never be reused.
  //
  // LINT.IfChange(ContextBackendUma)
  enum class ContextBackendUma {
    kNotSupported = 0,
    kCoreML = 1,
    kTFLite = 2,
    kLiteRT = 3,
    kONNXRuntime = 4,
    kDirectML_Obsolete = 5,
    kMaxValue = kDirectML_Obsolete,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:ContextBackendUma)

  static void RecordContextBackendUma(ContextBackendUma backend_uma);

  using CreateGraphImplCallback = base::OnceCallback<void(
      base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr>)>;

  using WebNNContextImplPtr =
      std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>;

  WebNNContextImpl(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      WebNNContextImpl::ContextBackendUma backend_uma,
      ContextProperties properties,
      mojom::CreateContextOptionsPtr options,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      std::unique_ptr<ScopedGpuSequence> gpu_sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // Constructor for running without GPU dependencies (e.g., TFLite in the
  // renderer process).
  WebNNContextImpl(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<tflite::ContextProviderTflite> tflite_context_provider,
      WebNNContextImpl::ContextBackendUma backend_uma,
      ContextProperties properties,
      mojom::CreateContextOptionsPtr options,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  WebNNContextImpl(const WebNNContextImpl&) = delete;
  WebNNContextImpl& operator=(const WebNNContextImpl&) = delete;

  virtual base::WeakPtr<WebNNContextImpl> AsWeakPtr() = 0;

  // Disassociates a `WebNNTensor` instance owned by this context by its handle.
  // Called when a `WebNNTensor` instance has a connection error. After this
  // call, it is no longer safe to use the WebNNTensorImpl.
  void RemoveWebNNTensorImpl(const blink::WebNNTensorToken& handle);

  // Disassociates a `WebNNGraph` instance owned by this context by its handle.
  // Called when a `WebNNGraph` instance has a connection error. After this
  // call, it is no longer safe to use the WebNNGraphImpl.
  void RemoveWebNNGraphImpl(const blink::WebNNGraphToken& handle);

  // Retrieves a `WebNNTensorImpl` instance created from this context.
  // Emits a bad message if a tensor with the given handle does not exist.
  scoped_refptr<WebNNTensorImpl> GetWebNNTensorImpl(
      const blink::WebNNTensorToken& handle);

  // Report the currently dispatching Message as bad and remove the GraphBuilder
  // receiver which received it.
  void ReportBadGraphBuilderMessage(
      const std::string& message,
      base::PassKey<WebNNGraphBuilderImpl> pass_key) override;

  // This method will be called by `WebNNGraphBuilderImpl::CreateGraph()` after
  // `graph_info` is validated. A backend subclass should implement this method
  // to build and compile a platform specific graph asynchronously.
  //
  // TODO(crbug.com/354724062): Move this to either `WebNNGraphImpl` or
  // `WebNNGraphBuilderImpl`.
  virtual void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
          constant_tensor_operands,
      CreateGraphImplCallback callback) = 0;

  // Called by a graph builder to destroy itself.
  void RemoveGraphBuilder(
      mojo::ReceiverId graph_builder_id,
      base::PassKey<WebNNGraphBuilderImpl> pass_key) override;

  // Get context properties with op support limits that are intersection
  // between WebNN generic limits and backend specific limits.
  static ContextProperties IntersectWithBaseProperties(
      ContextProperties backend_context_properties);

  const ContextProperties& properties() const override;
  const mojom::CreateContextOptions& options() const override;

  // GraphBuilderContext:
  void BuildGraph(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
          constant_tensor_operands,
      BuildGraphCallback callback) override;

  // Closes the `receiver_` pipe with the renderer process, then self destructs
  // by removing itself from the ownership of `context_provider_`.
  void OnLost(const std::string& reason);

  // Exposes a SequencedTaskRunner which can be used to schedule tasks in
  // sequence with this WebNNContext -- that is, on the same gpu::Scheduler
  // sequence. When running without a GPU sequence, returns the owning task
  // runner. Does not support nested loops or delayed tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

  // Exposes the ScopedGpuSequence which can be used to schedule tasks
  // in sequence with this WebNNContext -- that is, on the same gpu::Scheduler
  // sequence. May be null when running without GPU dependencies.
  ScopedGpuSequence* gpu_sequence() const;

  // Returns true if the data pipe consumer handle for WriteTensor() is valid.
  bool HasValidWriteTensorConsumer() const;

  // Returns true if the data pipe producer handle for ReadTensor() is valid.
  bool HasValidReadTensorProducer() const;

  // Reads data from either a BigBuffer or the data pipe into the provided span.
  void ReadDataFromBigBufferOrDataPipe(mojo_base::BigBuffer src_buffer,
                                       base::span<uint8_t> dst_span);

  // Writes data from the given span into the data pipe producer if it is valid
  // and return an empty BigBuffer, or into a new BigBuffer otherwise.
  mojo_base::BigBuffer WriteDataToDataPipeOrBigBuffer(
      base::span<const uint8_t> src_span);

  base::SequencedTaskRunner* owning_task_runner() {
    return owning_task_runner_.get();
  }

  // Defines a "transparent" comparator so that std::unique_ptr keys to
  // WebNNContextImpl instances can be compared against tokens for lookup in
  // associative containers like base::flat_set.
  struct Comparator {
    using is_transparent = blink::WebNNContextToken;

    bool operator()(const WebNNContextImplPtr& lhs,
                    const WebNNContextImplPtr& rhs) const {
      return lhs->handle() < rhs->handle();
    }

    bool operator()(const blink::WebNNContextToken& lhs,
                    const WebNNContextImplPtr& rhs) const {
      return lhs < rhs->handle();
    }

    bool operator()(const WebNNContextImplPtr& lhs,
                    const blink::WebNNContextToken& rhs) const {
      return lhs->handle() < rhs;
    }
  };

  // Exposes the task runner used to post tasks to the main thread.
  // Used for shared image operations that must run on the main thread.
  const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner() const {
    return main_task_runner_;
  }

  // Runs `task` directly when already on the target sequence, otherwise
  // schedules it on the context's GPU sequence. When there is no GPU sequence,
  // runs on the owning task runner. This allows arbitrary logic to be safely
  // executed on the context's task runner. The context is guaranteed to remain
  // alive for the duration of the task.
  void RunOrScheduleTask(
      base::OnceClosure task,
      const gpu::SyncToken& fence = gpu::SyncToken(),
      const gpu::SyncToken& release_token = gpu::SyncToken());

  int tracing_id() const { return tracing_id_; }

  virtual std::string_view GetBackendName() const = 0;

  virtual std::vector<mojom::WebNNExecutionProviderDetailsPtr>
  GetExecutionProvidersInfo() const = 0;

 protected:
  friend struct OnTaskRunnerDeleter;

  ~WebNNContextImpl() override;

  // mojom::WebNNContext
  void CreateGraphBuilder(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraphBuilder> receiver)
      override;
  void CreateTensor(mojom::TensorInfoPtr tensor_info,
                    mojo_base::BigBuffer tensor_data,
                    CreateTensorCallback callback) override;
  void CreateTensorFromMailbox(mojom::TensorInfoPtr tensor_info,
                               const gpu::Mailbox& mailbox,
                               const gpu::SyncToken& fence,
                               CreateTensorCallback callback) override;
  void Dispatch(
      const blink::WebNNGraphToken& graph_token,
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
      const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs)
      override;

  // This method will be called by `CreateTensor()` after the tensor info is
  // validated. A backend subclass should implement this method to create and
  // initialize a platform specific tensor.
  virtual base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   mojom::TensorInfoPtr tensor_info) = 0;

  // Similar to `CreateTensorImpl()`, but creates a tensor from a shared image
  // for WebGPU interop. Backend subclasses should implement this to
  // asynchronously create a platform-specific tensor from a shared image.
  virtual base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorFromSharedImageImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNTensorImpl::RepresentationPtr representation) = 0;

#if BUILDFLAG(IS_WIN)
  // Kill the GPU process to destroy all contexts.
  void DestroyAllContextsAndKillGpuProcess();
#endif  // BUILDFLAG(IS_WIN)

  void CreateWeightsFile(base::OnceCallback<void(base::File)> callback);

  // This weak pointer can only be dereferenced on the sequence where
  // `context_provider_->main_thread_task_runner()` runs tasks.
  base::WeakPtr<WebNNContextProviderImpl> context_provider_;

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // For tflite renderer process. Only dereference on main_task_runner_.
  base::WeakPtr<tflite::ContextProviderTflite> tflite_context_provider_;

  // True when this context is owned by a ContextProviderTflite (renderer
  // process). This flag is thread-safe to read since it is set at construction
  // and never modified.
  const bool is_tflite_context_provider_ = false;
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  // Context properties reported to the renderer process.
  const ContextProperties properties_;

  // Configuration options provided by the renderer process when creating this
  // context.
  mojom::CreateContextOptionsPtr options_;

  // The MemoryTypeTracker is used to track creation of tensors.
  // It is stored on the context because only the tracker it was created with
  // is thread safe.
  gpu::MemoryTypeTracker memory_type_tracker_;

  // TensorImpls owned by the context so the WebNN service can look them up
  // by token and use them during MLContext operations from the renderer
  // process. This cache only contains valid TensorImpls whose size is managed
  // by the lifetime of the tensors it contains.
  base::flat_set<
      scoped_refptr<WebNNTensorImpl>,
      WebNNObjectImpl<mojom::WebNNTensor,
                      blink::WebNNTensorToken,
                      mojo::AssociatedReceiver<mojom::WebNNTensor>>::Comparator>
      tensor_impls_;

 private:
  friend class base::DeleteHelper<WebNNContextImpl>;

  // Common initialization shared by both constructors.
  void InitializeContext(ContextBackendUma backend_uma);

  void OnDisconnect() override;

  // Callback for BuildGraph. Takes ownership of the graph and
  // extracts the devices for the builder.
  void OnGraphBuilt(
      BuildGraphCallback callback,
      base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr> result);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  using RunOrScheduleTaskCallback = base::OnceCallback<void(WebNNContextImpl&)>;
  void RunOrScheduleTaskWithThisContext(
      RunOrScheduleTaskCallback task,
      const gpu::SyncToken& fence = gpu::SyncToken());

  // Graph builders owned by this context.
  mojo::UniqueAssociatedReceiverSet<mojom::WebNNGraphBuilder>
      graph_builder_impls_;

  // GraphImpls owned by the context. Graphs use a WeakPtr to safely access the
  // context during build operations.
  base::flat_set<
      scoped_refptr<WebNNGraphImpl>,
      WebNNObjectImpl<mojom::WebNNGraph,
                      blink::WebNNGraphToken,
                      mojo::AssociatedReceiver<mojom::WebNNGraph>>::Comparator>
      graph_impls_;

  // WebNN context API operations execute tasks in a sequence.
  // Within a WebNN context, tasks are orderered, but remain async with respect
  // to tasks in other WebNN contexts or sequences.
  std::unique_ptr<ScopedGpuSequence> gpu_sequence_;

  // Data pipe handles for transferring tensor data across processes.
  mojo::ScopedDataPipeConsumerHandle write_tensor_consumer_;
  mojo::ScopedDataPipeProducerHandle read_tensor_producer_;

  // The SharedImageManager is used for creating tensors from shared images.
  // May be null when running without GPU dependencies.
  //
  // It is provided by the provider but stored per context, because only the
  // SharedImageManager is thread-safe.
  //
  // Storing a raw pointer is safe because WebNNContextImpl is owned by its
  // provider and cannot outlive it, while the SharedImageManager is managed by
  // the GPU service and destroyed after the provider, ensuring the raw pointer
  // remains valid.
  const raw_ptr<gpu::SharedImageManager> shared_image_manager_ = nullptr;

  // Task runner used to remove this context from its provider.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The owning_task_runner is the underlying task runner for the context.
  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner_;

  // A process-unique ID used for disambiguating memory dumps from different
  // WebNN contexts.
  const int tracing_id_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_IMPL_H_
