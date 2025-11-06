// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/oop_video_decoder_factory_process_service.h"

#include "base/functional/bind.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/media_switches.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace media {

OOPVideoDecoderFactoryProcessService::OOPVideoDecoderFactoryProcessService(
    mojo::PendingReceiver<mojom::VideoDecoderFactoryProcess> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OOPVideoDecoderFactoryProcessService::~OOPVideoDecoderFactoryProcessService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_image_interface_ && !shared_image_interface_->IsLost()) {
    shared_image_interface_->RemoveGpuChannelLostObserver(this);
  }
}

void OOPVideoDecoderFactoryProcessService::InitializeVideoDecoderFactory(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gpu_remote.is_valid()) {
    viz_gpu_ = viz::Gpu::Create(std::move(gpu_remote), io_task_runner_);
  }

  // The browser process ensures this is called only once.
  DCHECK(!factory_);
  factory_ = std::make_unique<OOPVideoDecoderFactoryService>(
      gpu_feature_info, GetSharedImageInterface());

  // base::Unretained(this) is safe here because the disconnection callback
  // won't run beyond the lifetime of |factory_| which is fully owned by
  // *|this|.
  factory_->BindReceiver(
      std::move(receiver),
      base::BindOnce(
          &OOPVideoDecoderFactoryProcessService::OnFactoryDisconnected,
          base::Unretained(this)));
}

void OOPVideoDecoderFactoryProcessService::OnGpuChannelLost() {
  // GpuChannel lost is notified on utility IO thread. Forward it to utility
  // main and allow notification to finish.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OOPVideoDecoderFactoryProcessService::OnGpuChannelLostTask,
          weak_ptr_factory_.GetWeakPtr()));
}

void OOPVideoDecoderFactoryProcessService::OnGpuChannelLostTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!shared_image_interface_) {
    return;
  }

  // The Observable removes all observers after completing GpuChannelLost
  // notification. No need to RemoveObserver(). Call RemoveObserver during
  // notification will cause deadlock.
  factory_->OnGpuChannelReestablished(GetSharedImageInterface());
}

scoped_refptr<gpu::SharedImageInterface>
OOPVideoDecoderFactoryProcessService::GetSharedImageInterface() {
  if (!viz_gpu_) {
    return nullptr;
  }
  if (shared_image_interface_ && !shared_image_interface_->IsLost()) {
    return shared_image_interface_;
  }
  shared_image_interface_.reset();
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      viz_gpu_->EstablishGpuChannelSync();
  if (!gpu_channel_host ||
      !gpu_channel_host->AddObserverIfNotAlreadyLost(this)) {
    return nullptr;
  }
  shared_image_interface_ =
      gpu_channel_host->CreateClientSharedImageInterface();
  return shared_image_interface_;
}

void OOPVideoDecoderFactoryProcessService::OnFactoryDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This should cause the termination of the utility process that *|this| lives
  // in.
  receiver_.reset();
}

}  // namespace media
