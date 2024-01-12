// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/dcomp_surface_registry_broker.h"

#include "base/logging.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {

DCOMPSurfaceRegistryBroker::DCOMPSurfaceRegistryBroker() = default;

DCOMPSurfaceRegistryBroker::~DCOMPSurfaceRegistryBroker() = default;

void DCOMPSurfaceRegistryBroker::RegisterDCOMPSurfaceHandle(
    mojo::PlatformHandle surface_handle,
    RegisterDCOMPSurfaceHandleCallback callback) {
  DVLOG(1) << __func__;

  auto* gpu_process_host =
      GpuProcessHost::Get(GpuProcessKind::GPU_PROCESS_KIND_SANDBOXED, false);
  if (!gpu_process_host) {
    DLOG(ERROR) << __func__ << ": Failed to get GpuProcessHost!";
    std::move(callback).Run(base::UnguessableToken());
    return;
  }

  auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
  gpu_service->RegisterDCOMPSurfaceHandle(
      std::move(surface_handle), mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                     std::move(callback), std::nullopt));
}

void DCOMPSurfaceRegistryBroker::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  DVLOG(1) << __func__;

  auto* gpu_process_host =
      GpuProcessHost::Get(GpuProcessKind::GPU_PROCESS_KIND_SANDBOXED, false);
  if (!gpu_process_host) {
    DLOG(ERROR) << __func__ << ": Failed to get GpuProcessHost!";
    return;
  }

  auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
  gpu_service->UnregisterDCOMPSurfaceHandle(token);
}

}  // namespace content
