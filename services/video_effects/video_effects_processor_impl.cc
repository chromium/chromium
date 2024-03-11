// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_impl.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace video_effects {

VideoEffectsProcessorImpl::VideoEffectsProcessorImpl(
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver)
    : manager_remote_(std::move(manager_remote)),
      processor_receiver_(this, std::move(processor_receiver)) {}

VideoEffectsProcessorImpl::~VideoEffectsProcessorImpl() = default;

void VideoEffectsProcessorImpl::PostProcess(
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    PostProcessCallback callback) {
  std::move(callback).Run(
      mojom::PostProcessResult::NewError(mojom::PostProcessError::kUnknown));
}

}  // namespace video_effects
