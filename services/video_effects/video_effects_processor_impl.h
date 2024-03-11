// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_

#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace video_effects {

class VideoEffectsProcessorImpl : public mojom::VideoEffectsProcessor {
 public:
  explicit VideoEffectsProcessorImpl(
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager_remote,
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor_receiver);

  ~VideoEffectsProcessorImpl() override;

  void PostProcess(media::mojom::VideoBufferHandlePtr input_frame_data,
                   media::mojom::VideoFrameInfoPtr input_frame_info,
                   media::mojom::VideoBufferHandlePtr result_frame_data,
                   media::VideoPixelFormat result_pixel_format,
                   PostProcessCallback callback) override;

 private:
  mojo::Remote<media::mojom::VideoEffectsManager> manager_remote_;
  mojo::Receiver<mojom::VideoEffectsProcessor> processor_receiver_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_IMPL_H_
