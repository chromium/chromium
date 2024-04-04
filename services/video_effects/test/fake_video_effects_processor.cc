// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/test/fake_video_effects_processor.h"

#include "base/functional/bind.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-shared.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace video_effects {

FakeVideoEffectsProcessor::FakeVideoEffectsProcessor(
    mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor,
    mojo::PendingRemote<media::mojom::VideoEffectsManager> manager)
    : receiver_(this, std::move(processor)), manager_(std::move(manager)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&FakeVideoEffectsProcessor::OnMojoConnectionLost,
                     weak_ptr_factory_.GetWeakPtr()));
  manager_.set_disconnect_handler(
      base::BindOnce(&FakeVideoEffectsProcessor::OnMojoConnectionLost,
                     weak_ptr_factory_.GetWeakPtr()));
}

FakeVideoEffectsProcessor::~FakeVideoEffectsProcessor() = default;

void FakeVideoEffectsProcessor::PostProcess(
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    PostProcessCallback callback) {
  std::move(callback).Run(
      mojom::PostProcessResult::NewError(mojom::PostProcessError::kUnknown));
}

mojo::Remote<media::mojom::VideoEffectsManager>&
FakeVideoEffectsProcessor::GetVideoEffectsManager() {
  return manager_;
}

void FakeVideoEffectsProcessor::OnMojoConnectionLost() {
  receiver_.reset();
  manager_.reset();
}

}  // namespace video_effects
