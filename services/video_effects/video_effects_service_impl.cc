// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_service_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/video_effects/video_effects_processor_impl.h"
#include "services/video_effects/viz_gpu_channel_host_provider.h"

namespace video_effects {

VideoEffectsServiceImpl::VideoEffectsServiceImpl(
    mojo::PendingReceiver<mojom::VideoEffectsService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

VideoEffectsServiceImpl::~VideoEffectsServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoEffectsServiceImpl::CreateEffectsProcessor(
    const std::string& device_id,
    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (processors_.contains(device_id)) {
    return;
  }

  auto gpu = viz::Gpu::Create(std::move(gpu_remote), io_task_runner_);
  auto gpu_channel_host_provider =
      std::make_unique<VizGpuChannelHostProvider>(std::move(gpu));

  auto on_unrecoverable_processor_error =
      base::BindOnce(&VideoEffectsServiceImpl::RemoveProcessor,
                     weak_ptr_factory_.GetWeakPtr(), device_id);

  auto effects_processor = std::make_unique<VideoEffectsProcessorImpl>(
      std::move(manager_remote), std::move(processor_receiver),
      std::move(gpu_channel_host_provider),
      std::move(on_unrecoverable_processor_error));

  if (!effects_processor->Initialize()) {
    return;
  }

  auto [_, inserted] = processors_.insert(
      std::make_pair(device_id, std::move(effects_processor)));
  CHECK(inserted);
}

void VideoEffectsServiceImpl::SetBackgroundSegmentationModel(
    base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(bialpio): make a copy of the model blob to serve it every time new
  // processor gets created.
  base::MemoryMappedFile memory_mapped_model_file;
  if (!memory_mapped_model_file.Initialize(std::move(model_file))) {
    return;
  }

  // Propagate the model to all already existing processors:
  for (auto& device_id_and_processor : processors_) {
    device_id_and_processor.second->SetBackgroundSegmentationModel(
        memory_mapped_model_file.bytes());
  }
}

void VideoEffectsServiceImpl::RemoveProcessor(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  processors_.erase(id);
}

}  // namespace video_effects
