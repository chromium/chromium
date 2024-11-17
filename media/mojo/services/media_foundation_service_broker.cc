// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service_broker.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/mojo/services/media_foundation_gpu_info_monitor.h"

namespace media {
MediaFoundationServiceBroker::MediaFoundationServiceBroker(
    mojo::PendingReceiver<mojom::MediaFoundationServiceBroker> receiver,
    base::OnceClosure ensure_sandboxed_cb)
    : receiver_(this, std::move(receiver)),
      gpu_info_observer_(this),
      ensure_sandboxed_cb_(std::move(ensure_sandboxed_cb)) {}

MediaFoundationServiceBroker::~MediaFoundationServiceBroker() = default;

void MediaFoundationServiceBroker::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info,
    UpdateGpuInfoCallback callback) {
  OnGpuInfoUpdate(gpu_info);
  std::move(callback).Run(gpu_info_observer_.BindNewPipeAndPassRemote());
}

void MediaFoundationServiceBroker::GetService(
    const base::FilePath& cdm_path,
    mojo::PendingReceiver<mojom::MediaFoundationService> service_receiver) {
  DVLOG(1) << __func__ << ": cdm_path=" << cdm_path;

  if (media_foundation_service_) {
    DVLOG(1) << __func__ << ": MediaFoundationService can only be bound once";
    return;
  }

  bool success = MediaFoundationCdmModule::GetInstance()->Initialize(cdm_path);
  std::move(ensure_sandboxed_cb_).Run();

  media_foundation_service_ =
      std::make_unique<MediaFoundationService>(std::move(service_receiver));

  DVLOG(1) << __func__ << ": success=" << success;
  if (!success) {
    // We don't want to terminate the service immediately but with a delay so
    // that we can give a chance to the CDM creation operation to inspect the
    // actual reason. i.e., LoadCdm failed.
    const base::TimeDelta kServiceTerminationDelay = base::Seconds(5);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationServiceBroker::TerminateService,
                       weak_factory_.GetWeakPtr()),
        kServiceTerminationDelay);
  }
}

void MediaFoundationServiceBroker::OnGpuInfoUpdate(
    const gpu::GPUInfo& gpu_info) {
  // When the MediaFoundationService crashes, the GPUInfo will be available in
  // the crash report.
  DVLOG(1) << __func__;
  gpu::SetKeysForCrashLogging(gpu_info);
  MediaFoundationGpuInfoMonitor::GetInstance()->UpdateGpuInfo(gpu_info);
}

void MediaFoundationServiceBroker::TerminateService() {
  DVLOG(1) << __func__;
  DCHECK(media_foundation_service_);
  media_foundation_service_.reset();
}

}  // namespace media
