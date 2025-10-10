// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/types/optional_ref.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace webnn {

class ScopedSequence;
class WebNNContextImpl;

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
      LoseAllContextsCallback lose_all_contexts_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gpu::Scheduler* scheduler,
      int32_t client_id);

  // Called to add a another WebNNContextProvider receiver to this
  // existing `WebNNContextProviderImpl` instance.
  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver);

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
  // Send the contexts lost reason to the renderer process and kill the GPU
  // process to destroy all contexts.
  void DestroyAllContextsAndKillGpuProcess(const std::string& reason);
#endif  // BUILDFLAG(IS_WIN)

  // Retrieves a `WebNNContextImpl` instance created from this provider.
  // Emits a bad message if a context with the given handle does not exist.
  base::optional_ref<WebNNContextImpl> GetWebNNContextImplForTesting(
      const blink::WebNNContextToken& handle);

  using WebNNContextImplSet = base::flat_set<
      scoped_refptr<WebNNContextImpl>,
      WebNNObjectImpl<mojom::WebNNContext,
                      blink::WebNNContextToken,
                      mojo::Receiver<mojom::WebNNContext>>::Comparator>;

  // The test cases can override the context creating behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual scoped_refptr<WebNNContextImpl> CreateWebNNContext(
        base::WeakPtr<WebNNContextProviderImpl> context_provider_impl,
        mojom::CreateContextOptionsPtr options,
        gpu::CommandBufferId command_buffer_id,
        std::unique_ptr<ScopedSequence> sequence,
        scoped_refptr<gpu::MemoryTracker> memory_tracker,
        scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
        gpu::SharedImageManager* shared_image_manager,
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        CreateWebNNContextCallback callback) = 0;
  };

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

  int32_t client_id() const { return client_id_; }

  scoped_refptr<gpu::SharedContextState> shared_context_state() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return shared_context_state_;
  }

  // For tests: ensure that all WebNNContextImpls have been destroyed on their
  // owning task runners, since they may post tasks to the gpu::Scheduler.
  base::flat_set<scoped_refptr<base::SequencedTaskRunner>>
  GetAllContextTaskRunnersForTesting();

 protected:
  // SequenceChecker for WebNNContextProviderImpl. It attaches to the sequence
  // on which this object is constructed. All message dispatches and any access
  // to `main_thread_task_runner_` must happen on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  WebNNContextProviderImpl(
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info,
      gpu::GPUInfo gpu_info,
      gpu::SharedImageManager* shared_image_manager,
      LoseAllContextsCallback lose_all_contexts_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gpu::Scheduler* scheduler,
      int32_t client_id);

  // mojom::WebNNContextProvider
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override;

  base::WeakPtr<WebNNContextProviderImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Called after CreateWebNNContext successfully creates a `WebNNContextImpl`.
  // This associates the context with this provider on the specified sequence.
  void OnCreateWebNNContextImpl(
      WebNNContextProvider::CreateWebNNContextCallback callback,
      mojo::PendingRemote<::webnn::mojom::WebNNContext> remote,
      mojo::ScopedDataPipeProducerHandle write_tensor_producer,
      mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
      scoped_refptr<WebNNContextImpl> context_impl);

  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  const gpu::GpuFeatureInfo gpu_feature_info_;
  const gpu::GPUInfo gpu_info_;

  // The lifetime of the shared image manager is managed by the GPU service and
  // is destroyed after this WebNNProviderImpl is destroyed, which makes it
  // safe to store the shared image manager as a raw_ptr here.
  const raw_ptr<gpu::SharedImageManager> shared_image_manager_;

  // A callback from `GpuServiceImpl` to terminate the GPU process, which will
  // destroy all contexts.
  LoseAllContextsCallback lose_all_contexts_callback_;

  mojo::ReceiverSet<mojom::WebNNContextProvider> provider_receivers_;

  // Lifetime of the scheduler is managed by the GPU service. The GPU service
  // destroys the WebNNContextProviderImpl and all its contexts when it
  // is destroyed. So a raw pointer is safe. Must be destroyed after
  // WebNNContextImpl(s) since the scheduler is accessed for their destruction.
  const raw_ptr<gpu::Scheduler> scheduler_;

  // Contexts created by this provider. When a context disconnects,
  // it will destroy itself by removing itself from this set.
  WebNNContextImplSet context_impls_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Specifies the thread on which the GPU scheduler should run tasks.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  const int32_t client_id_;

  // The memory tracker from the `shared_context_state_` which is used to create
  // tensors from shared images.
  // TODO(crbug.com/345352987): give WebNN its own memory source and
  // tracker.
  scoped_refptr<gpu::MemoryTracker> memory_tracker_;

  base::WeakPtrFactory<WebNNContextProviderImpl> weak_factory_{this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
