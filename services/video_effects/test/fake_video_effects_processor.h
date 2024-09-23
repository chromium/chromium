// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_PROCESSOR_H_
#define SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_PROCESSOR_H_

#include "base/memory/weak_ptr.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace video_effects {

class FakeVideoEffectsProcessor : public mojom::VideoEffectsProcessor {
 public:
  explicit FakeVideoEffectsProcessor(
      mojo::PendingReceiver<mojom::VideoEffectsProcessor> processor,
      mojo::PendingRemote<media::mojom::VideoEffectsManager> manager);
  ~FakeVideoEffectsProcessor() override;

  // mojom::VideoEffectsProcessor implementation
  void PostProcess(media::mojom::VideoBufferHandlePtr input_frame_data,
                   media::mojom::VideoFrameInfoPtr input_frame_info,
                   media::mojom::VideoBufferHandlePtr result_frame_data,
                   media::VideoPixelFormat result_pixel_format,
                   PostProcessCallback callback) override;

  // For testing, get the manager that this processor will use to obtain the
  // video effects configuration:
  mojo::Remote<media::mojom::VideoEffectsManager>& GetVideoEffectsManager();

 private:
  void OnMojoConnectionLost();

  mojo::Receiver<mojom::VideoEffectsProcessor> receiver_;
  mojo::Remote<media::mojom::VideoEffectsManager> manager_;

  // Must be last:
  base::WeakPtrFactory<FakeVideoEffectsProcessor> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_TEST_FAKE_VIDEO_EFFECTS_PROCESSOR_H_
