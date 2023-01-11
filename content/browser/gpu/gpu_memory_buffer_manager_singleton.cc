// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

namespace content {
namespace {

GpuMemoryBufferManagerSingleton* g_gpu_memory_buffer_manager;

viz::mojom::GpuService* GetGpuService(
    base::OnceClosure connection_error_handler) {
  if (auto* host = GpuProcessHost::Get()) {
    host->gpu_host()->AddConnectionErrorHandler(
        std::move(connection_error_handler));
    return host->gpu_service();
  }
  return nullptr;
}

#if defined(USE_OZONE_PLATFORM_X11)
bool ShouldSetBufferFormatsFromGpuExtraInfo() {
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .fetch_buffer_formats_for_gmb_on_gpu;
}
#endif

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
#if BUILDFLAG(IS_MAC)
  return ui::WindowResizeHelperMac::Get()->task_runner();
#else
  return GetUIThreadTaskRunner({});
#endif
}

}  // namespace

GpuMemoryBufferManagerSingleton::GpuMemoryBufferManagerSingleton(int client_id)
    : HostGpuMemoryBufferManager(
          base::BindRepeating(&content::GetGpuService),
          client_id,
          std::make_unique<gpu::GpuMemoryBufferSupport>(),
          GetTaskRunner()),
      gpu_data_manager_impl_(GpuDataManagerImpl::GetInstance()) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
  gpu_data_manager_impl_->AddObserver(this);
}

GpuMemoryBufferManagerSingleton::~GpuMemoryBufferManagerSingleton() {
  DCHECK_EQ(this, g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = nullptr;
  gpu_data_manager_impl_->RemoveObserver(this);
}

// static
GpuMemoryBufferManagerSingleton*
GpuMemoryBufferManagerSingleton::GetInstance() {
  return g_gpu_memory_buffer_manager;
}

void GpuMemoryBufferManagerSingleton::OnGpuExtraInfoUpdate() {
#if defined(USE_OZONE_PLATFORM_X11)
  // X11 fetches buffer formats on gpu and passes them via gpu extra info.
  if (!ShouldSetBufferFormatsFromGpuExtraInfo())
    return;

  gpu::GpuMemoryBufferConfigurationSet configs;
  for (const auto& config : gpu_data_manager_impl_->GetGpuExtraInfo()
                                .gpu_memory_buffer_support_x11) {
    configs.insert(config);
  }
  SetNativeConfigurations(std::move(configs));
#endif
}

}  // namespace content
