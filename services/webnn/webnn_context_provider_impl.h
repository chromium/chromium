// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include "base/types/expected.h"
#endif

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace webnn {

class ScopedGpuSequence;

#if BUILDFLAG(IS_WIN)
namespace ort {
class Environment;
}
#endif

// Maintain a set of WebNNContextImpl instances that are created by the context
// provider.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextProviderImpl
    : public mojom::WebNNContextProvider {
 public:
  WebNNContextProviderImpl(const WebNNContextProviderImpl&) = delete;
  WebNNContextProviderImpl& operator=(const WebNNContextProviderImpl&) = delete;

  ~WebNNContextProviderImpl() override;

  using LoseAllContextsCallback = base::OnceCallback<void()>;

  // Called when the `WebNNContextProviderImpl` instance will be owned by
  // the GPU service and used to add additional WebNNContextProvider
  // receivers.
  static std::unique_ptr<WebNNContextProviderImpl> Create(
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info,
      gpu::GPUInfo gpu_info,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
      LoseAllContextsCallback lose_all_contexts_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gpu::Scheduler* scheduler,
      mojo::SharedRemote<viz::mojom::GpuHost> gpu_host);

  struct WebNNReceiversParams {
    // Indicates whether the provider is operating in incognito mode.
    const bool is_incognito;
    const int32_t client_id;
    const uint64_t client_tracing_id;
  };

  // Called to add a another WebNNContextProvider receiver to this
  // existing `WebNNContextProviderImpl` instance.
  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
      const WebNNReceiversParams& params);

  enum class WebNNStatus {
    kWebNNGpuDisabled = 0,
    kWebNNNpuDisabled = 1,
    kWebNNGpuFeatureStatusDisabled = 2,
    kWebNNEnabled = 3,
  };

  // Disassociates a `WebNNContextImpl` instance owned by this provider by its
  // handle. Called when a `WebNNContext` instance has a connection error. After
  // this call, it is no longer safe to use the WebNNContextImpl.
  void RemoveWebNNContextImpl(const blink::WebNNContextToken& handle);

#if BUILDFLAG(IS_WIN)
  // Kill the GPU process to destroy all contexts.
  void DestroyAllContextsAndKillGpuProcess();
#endif  // BUILDFLAG(IS_WIN)

  // Retrieves a `WebNNContextImpl` instance created from this provider.
  // Emits a bad message if a context with the given handle does not exist.
  base::optional_ref<WebNNContextImpl> GetWebNNContextImplForTesting(
      const blink::WebNNContextToken& handle);

  using WebNNContextImplPtr =
      std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>;
  using WebNNContextImplSet =
      base::flat_set<WebNNContextImplPtr, WebNNContextImpl::Comparator>;

  // The test cases can override the context creating behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual WebNNContextImplPtr CreateWebNNContext(
        base::WeakPtr<WebNNContextProviderImpl> context_provider_impl,
        mojom::CreateContextOptionsPtr options,
        std::unique_ptr<ScopedGpuSequence> gpu_sequence,
        scoped_refptr<gpu::MemoryTracker> memory_tracker,
        scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
        gpu::SharedImageManager* shared_image_manager,
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        CreateWebNNContextCallback callback) = 0;
  };

  void CreateWeightsFile(base::OnceCallback<void(base::File)> callback);

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

 private:
  WebNNContextProviderImpl(
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info,
      gpu::GPUInfo gpu_info,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
      LoseAllContextsCallback lose_all_contexts_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gpu::Scheduler* scheduler,
      mojo::SharedRemote<viz::mojom::GpuHost> gpu_host);

  // mojom::WebNNContextProvider
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override;

  base::WeakPtr<WebNNContextProviderImpl> AsWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

  // Called after CreateWebNNContext successfully creates a `WebNNContextImpl`.
  // This associates the context with this provider on the specified sequence.
  void OnCreateWebNNContextImpl(
      WebNNContextProvider::CreateWebNNContextCallback callback,
      mojo::PendingRemote<::webnn::mojom::WebNNContext> remote,
      mojo::ScopedDataPipeProducerHandle write_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
      WebNNContextImplPtr context_impl);

#if BUILDFLAG(WEBNN_USE_TFLITE)
  void CreateTFLiteContext(
      ScopedTrace scoped_trace,
      mojom::CreateContextOptionsPtr options,
      mojo::ScopedDataPipeProducerHandle write_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedGpuSequence> gpu_sequence,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      mojo::PendingRemote<mojom::WebNNContext> remote,
      CreateWebNNContextCallback callback,
      bool is_incognito,
      scoped_refptr<gpu::MemoryTracker> memory_tracker);
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

#if BUILDFLAG(WEBNN_USE_LITERT)
  void CreateLiteRtContext(
      ScopedTrace scoped_trace,
      mojom::CreateContextOptionsPtr options,
      mojo::ScopedDataPipeProducerHandle write_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedGpuSequence> gpu_sequence,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      mojo::PendingRemote<mojom::WebNNContext> remote,
      CreateWebNNContextCallback callback);
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(IS_WIN)
  void OnOrtEnvCreated(ScopedTrace scoped_trace,
                       mojom::CreateContextOptionsPtr options,
                       mojo::ScopedDataPipeProducerHandle write_tensor_producer,
                       mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
                       mojo::ScopedDataPipeProducerHandle read_tensor_producer,
                       mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
                       gpu::CommandBufferId command_buffer_id,
                       std::unique_ptr<ScopedGpuSequence> gpu_sequence,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       mojo::PendingReceiver<mojom::WebNNContext> receiver,
                       mojo::PendingRemote<mojom::WebNNContext> remote,
                       CreateWebNNContextCallback callback,
                       bool is_incognito,
                       scoped_refptr<gpu::MemoryTracker> memory_tracker,
                       base::expected<scoped_refptr<ort::Environment>,
                                      std::string> env_creation_results);

  void DidEnsureWebNNExecutionProvidersReady(
      ScopedTrace scoped_trace,
      mojom::CreateContextOptionsPtr options,
      mojo::ScopedDataPipeProducerHandle write_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
      mojo::ScopedDataPipeProducerHandle read_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedGpuSequence> gpu_sequence,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      mojo::PendingRemote<mojom::WebNNContext> remote,
      CreateWebNNContextCallback callback,
      bool is_incognito,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info);
#endif  // BUILDFLAG(IS_WIN)

  scoped_refptr<gpu::SharedContextState> shared_context_state_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  const gpu::GpuFeatureInfo gpu_feature_info_;
  const gpu::GPUInfo gpu_info_;

  // The lifetime of the shared image manager is managed by the GPU service and
  // is destroyed after this WebNNProviderImpl is destroyed, which makes it
  // safe to store the shared image manager as a raw_ptr here.
  const raw_ptr<gpu::SharedImageManager> shared_image_manager_;

  // A callback from `GpuServiceImpl` to terminate the GPU process, which will
  // destroy all contexts.
  LoseAllContextsCallback lose_all_contexts_callback_;

  // Receivers for the WebNNContextProvider interface.
  // The context value indicates the parameters needed by the webnn context.
  mojo::ReceiverSet<mojom::WebNNContextProvider, WebNNReceiversParams>
      provider_receivers_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // Lifetime of the scheduler is managed by the GPU service. The GPU service
  // destroys the WebNNContextProviderImpl and all its contexts when it
  // is destroyed. So a raw pointer is safe. Must be destroyed after
  // WebNNContextImpl(s) since the scheduler is accessed for their destruction.
  const raw_ptr<gpu::Scheduler> scheduler_;

  // Contexts created by this provider. When a context disconnects,
  // it will destroy itself by removing itself from this set.
  WebNNContextImplSet context_impls_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // Specifies the thread on which the GPU scheduler should run tasks.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  const scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor_;

  mojo::SharedRemote<viz::mojom::GpuHost> gpu_host_;

  // SequenceChecker for WebNNContextProviderImpl. It attaches to the sequence
  // on which this object is constructed. All message dispatches and any access
  // to `main_thread_task_runner_` must happen on the same sequence.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<WebNNContextProviderImpl> weak_factory_
      GUARDED_BY_CONTEXT(main_sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
