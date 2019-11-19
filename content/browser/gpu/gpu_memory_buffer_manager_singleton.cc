// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

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

}  // namespace

GpuMemoryBufferManagerSingleton::GpuMemoryBufferManagerSingleton(int client_id)
    : HostGpuMemoryBufferManager(
          base::BindRepeating(&content::GetGpuService),
          client_id,
          std::make_unique<gpu::GpuMemoryBufferSupport>(),
          base::CreateSingleThreadTaskRunner({BrowserThread::IO})) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
}

GpuMemoryBufferManagerSingleton::~GpuMemoryBufferManagerSingleton() {
  DCHECK_EQ(this, g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = nullptr;
}

// static
GpuMemoryBufferManagerSingleton*
GpuMemoryBufferManagerSingleton::GetInstance() {
  return g_gpu_memory_buffer_manager;
}

}  // namespace content
