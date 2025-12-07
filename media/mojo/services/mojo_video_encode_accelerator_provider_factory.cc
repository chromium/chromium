// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_provider_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"

namespace media {

MojoVideoEncodeAcceleratorProviderFactory::
    MojoVideoEncodeAcceleratorProviderFactory()
    : receiver_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MojoVideoEncodeAcceleratorProviderFactory::
    ~MojoVideoEncodeAcceleratorProviderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoVideoEncodeAcceleratorProviderFactory::BindReceiver(
    mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProviderFactory>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

void MojoVideoEncodeAcceleratorProviderFactory::
    CreateVideoEncodeAcceleratorProvider(
        mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/248540499): pass useful gpu::GpuPreferences,
  // gpu::GpuDriverBugWorkarounds, and gpu::GPUInfo::GPUDevice instances.
  std::unique_ptr<mojom::VideoEncodeAcceleratorProvider> provider =
      std::make_unique<MojoVideoEncodeAcceleratorProvider>(
          base::BindRepeating(&GpuVideoEncodeAcceleratorFactory::CreateVEA),
          gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
          gpu::GPUInfo::GPUDevice(), /*media_gpu_channel_manager=*/nullptr,
          base::SingleThreadTaskRunner::GetCurrentDefault());

  video_encoder_providers_.Add(std::move(provider), std::move(receiver));
}

}  // namespace media