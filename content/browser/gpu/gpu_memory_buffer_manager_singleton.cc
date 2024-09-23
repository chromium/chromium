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
#include "ui/base/ozone_buildflags.h"
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

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
#if BUILDFLAG(IS_MAC)
  return ui::WindowResizeHelperMac::Get()->task_runner();
#else
  return GetUIThreadTaskRunner({});
#endif
}

#if BUILDFLAG(IS_LINUX)
bool IsGpuMemoryBufferNV12Supported() {
  static bool is_computed = false;
  static bool supported = false;
  if (is_computed) {
    return supported;
  }

  auto* gmb_mgr = GpuMemoryBufferManagerSingleton::GetInstance();
  if (gmb_mgr) {
    auto gmb = gmb_mgr->CreateGpuMemoryBuffer(
        gfx::Size(2, 2), gfx::BufferFormat::YUV_420_BIPLANAR,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE, gpu::kNullSurfaceHandle,
        nullptr);
    if (gmb && gmb->GetType() == gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
      supported = true;
    }
  }

  is_computed = true;

  return supported;
}
#endif  // BUILDFLAG(IS_LINUX)

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
#if BUILDFLAG(IS_LINUX)
  // Dynamic check whether the NV12 format is supported as it may be
  // inconsistent between the system GBM (Generic Buffer Management) and
  // chromium miniGBM.
  gpu_data_manager_impl_->SetGpuMemoryBufferNV12Supported(
      IsGpuMemoryBufferNV12Supported());
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace content
