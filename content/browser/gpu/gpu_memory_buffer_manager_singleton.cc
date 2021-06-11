// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "build/build_config.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/base/ui_base_features.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#elif defined(OS_MAC)
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

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
bool ShouldSetBufferFormatsFromGpuExtraInfo() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return ui::OzonePlatform::GetInstance()
        ->GetPlatformProperties()
        .fetch_buffer_formats_for_gmb_on_gpu;
  }
#endif
  return true;
}
#endif

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
  if (!base::FeatureList::IsEnabled(features::kProcessHostOnUI))
    return GetIOThreadTaskRunner({});

#if defined(OS_MAC)
  return ui::WindowResizeHelperMac::Get()->task_runner();
#endif

  return GetUIThreadTaskRunner({});
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
#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
  // X11 and non-Ozone/X11 fetch buffer formats on gpu and pass them via gpu
  // extra info.
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
