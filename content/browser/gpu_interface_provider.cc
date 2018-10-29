// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu_interface_provider.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_client.h"
#include "content/public/browser/gpu_interface_provider_factory.h"
#include "content/public/browser/gpu_service_registry.h"
#include "services/ws/public/mojom/gpu.mojom.h"

namespace content {

// InterfaceBinderImpl handles the actual binding. The binding has to happen on
// the IO thread.
class GpuInterfaceProvider::InterfaceBinderImpl
    : public base::RefCountedThreadSafe<InterfaceBinderImpl> {
 public:
  InterfaceBinderImpl() = default;

  void BindGpuRequestOnGpuTaskRunner(ws::mojom::GpuRequest request) {
    // The GPU task runner is bound to the IO thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    auto gpu_client = content::CreateGpuClient(
        std::move(request),
        base::BindOnce(&InterfaceBinderImpl::OnGpuClientConnectionError, this),
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO}));
    gpu_clients_.push_back(std::move(gpu_client));
  }

  void BindDiscardableSharedMemoryManagerOnGpuTaskRunner(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest
          request) {
    content::BindInterfaceInGpuProcess(std::move(request));
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceBinderImpl>;
  ~InterfaceBinderImpl() = default;

  void OnGpuClientConnectionError(viz::GpuClient* client) {
    base::EraseIf(
        gpu_clients_,
        base::UniquePtrMatcher<viz::GpuClient, base::OnTaskRunnerDeleter>(
            client));
  }

  std::vector<std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter>>
      gpu_clients_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceBinderImpl);
};

GpuInterfaceProvider::GpuInterfaceProvider()
    : interface_binder_impl_(base::MakeRefCounted<InterfaceBinderImpl>()) {}

GpuInterfaceProvider::~GpuInterfaceProvider() = default;

void GpuInterfaceProvider::RegisterGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO});
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::
                              BindDiscardableSharedMemoryManagerOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::BindGpuRequestOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
}

#if defined(USE_OZONE)
void GpuInterfaceProvider::RegisterOzoneGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  // Registers the gpu-related interfaces needed by Ozone.
  // TODO(rjkroege): Adjust when Ozone/DRM/Mojo is complete.
  NOTIMPLEMENTED();
}
#endif

// Factory.
std::unique_ptr<ws::GpuInterfaceProvider> CreateGpuInterfaceProvider() {
  return std::make_unique<GpuInterfaceProvider>();
}

}  // namespace content
