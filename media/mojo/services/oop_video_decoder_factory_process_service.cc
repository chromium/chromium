// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/oop_video_decoder_factory_process_service.h"

namespace media {

OOPVideoDecoderFactoryProcessService::OOPVideoDecoderFactoryProcessService(
    mojo::PendingReceiver<mojom::VideoDecoderFactoryProcess> receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OOPVideoDecoderFactoryProcessService::~OOPVideoDecoderFactoryProcessService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OOPVideoDecoderFactoryProcessService::InitializeVideoDecoderFactory(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The browser process ensures this is called only once.
  DCHECK(!factory_);
  factory_ = std::make_unique<OOPVideoDecoderFactoryService>(gpu_feature_info);

  // base::Unretained(this) is safe here because the disconnection callback
  // won't run beyond the lifetime of |factory_| which is fully owned by
  // *|this|.
  factory_->BindReceiver(
      std::move(receiver),
      base::BindOnce(
          &OOPVideoDecoderFactoryProcessService::OnFactoryDisconnected,
          base::Unretained(this)));
}

void OOPVideoDecoderFactoryProcessService::OnFactoryDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This should cause the termination of the utility process that *|this| lives
  // in.
  receiver_.reset();
}

}  // namespace media
