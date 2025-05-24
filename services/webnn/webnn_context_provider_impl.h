// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/types/optional_ref.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace webnn {

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

  // Called to create a WebNNContextProviderImpl as a self-owned receiver.
  // Optionally returns a reference to the WebNNContextProviderImpl to test
  // interop.
  static base::optional_ref<WebNNContextProviderImpl> CreateForTesting(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
      WebNNStatus status = WebNNStatus::kWebNNEnabled,
      LoseAllContextsCallback lose_all_contexts_callback = base::BindOnce([]() {
      }));

  // Called when a WebNNContextImpl has a connection error. After this call, it
  // is no longer safe to access |impl|.
  void OnConnectionError(WebNNContextImpl* impl);

#if BUILDFLAG(IS_WIN)
  // Send the contexts lost reason to the renderer process and kill the GPU
  // process to destroy all contexts.
  void DestroyContextsAndKillGpuProcess(std::string_view reason);
#endif  // BUILDFLAG(IS_WIN)

  // Retrieves a `WebNNContextImpl` instance created from this provider.
  // Emits a bad message if a context with the given handle does not exist.
  base::optional_ref<WebNNContextImpl> GetWebNNContextImplForTesting(
      const blink::WebNNContextToken& handle);

  using WebNNContextImplSet = base::flat_set<
      std::unique_ptr<WebNNContextImpl>,
      WebNNObjectImpl<blink::WebNNContextToken>::Comparator<WebNNContextImpl>>;

  // The test cases can override the context creating behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual std::unique_ptr<WebNNContextImpl> CreateWebNNContext(
        WebNNContextProviderImpl* context_provider_impl,
        mojom::CreateContextOptionsPtr options,
        CreateWebNNContextCallback callback) = 0;
  };

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

  gpu::Scheduler* scheduler() const { return scheduler_; }

  int32_t client_id() const { return client_id_; }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner() const {
    return main_thread_task_runner_;
  }

 private:
  WebNNContextProviderImpl(
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info,
      gpu::GPUInfo gpu_info,
      LoseAllContextsCallback lose_all_contexts_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gpu::Scheduler* scheduler,
      int32_t client_id);

  // mojom::WebNNContextProvider
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override;

  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  const gpu::GpuFeatureInfo gpu_feature_info_;
  const gpu::GPUInfo gpu_info_;
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
  WebNNContextImplSet impls_;

  // Specifies the thread on which the GPU scheduler should run tasks.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  const int32_t client_id_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
