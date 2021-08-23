// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/dcomp_surface_registry_broker.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {

namespace {

void RegisterDCOMPSurfaceHandleInternal(
    mojo::PlatformHandle surface_handle,
    DCOMPSurfaceRegistryBroker::RegisterDCOMPSurfaceHandleCallback callback) {
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
                                     std::move(callback), absl::nullopt));
}

void UnregisterDCOMPSurfaceHandleInternal(const base::UnguessableToken& token) {
  auto* gpu_process_host =
      GpuProcessHost::Get(GpuProcessKind::GPU_PROCESS_KIND_SANDBOXED, false);
  if (!gpu_process_host) {
    DLOG(ERROR) << __func__ << ": Failed to get GpuProcessHost!";
    return;
  }

  auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
  gpu_service->UnregisterDCOMPSurfaceHandle(token);
}

}  // namespace

DCOMPSurfaceRegistryBroker::DCOMPSurfaceRegistryBroker() = default;

DCOMPSurfaceRegistryBroker::~DCOMPSurfaceRegistryBroker() = default;

void DCOMPSurfaceRegistryBroker::RegisterDCOMPSurfaceHandle(
    mojo::PlatformHandle surface_handle,
    RegisterDCOMPSurfaceHandleCallback callback) {
  DVLOG(1) << __func__;

  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
    RegisterDCOMPSurfaceHandleInternal(std::move(surface_handle),
                                       std::move(callback));
  } else {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RegisterDCOMPSurfaceHandleInternal,
                       std::move(surface_handle),
                       media::BindToCurrentLoop(std::move(callback))));
  }
}

void DCOMPSurfaceRegistryBroker::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  DVLOG(1) << __func__;

  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
    UnregisterDCOMPSurfaceHandleInternal(token);
  } else {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&UnregisterDCOMPSurfaceHandleInternal, token));
  }
}

}  // namespace content
