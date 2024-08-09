// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/test/fake_image_capturer.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

void FakeImageCapture::RegisterBinding(ExecutionContext* context) {
  DynamicTo<LocalDOMWindow>(context)
      ->GetBrowserInterfaceBroker()
      .SetBinderForTesting(media::mojom::blink::ImageCapture::Name_,
                           WTF::BindRepeating(&FakeImageCapture::Bind,
                                              weak_factory_.GetWeakPtr()));
}

void FakeImageCapture::Bind(mojo::ScopedMessagePipeHandle handle) {
  receivers_.Add(this, mojo::PendingReceiver<media::mojom::blink::ImageCapture>(
                           std::move(handle)));
}

void FakeImageCapture::GetPhotoState(const WTF::String& source_id,
                                     GetPhotoStateCallback callback) {
  media::mojom::blink::PhotoStatePtr photo_capabilities =
      media::mojom::blink::PhotoState::New();
  photo_capabilities->height = media::mojom::blink::Range::New();
  photo_capabilities->width = media::mojom::blink::Range::New();
  photo_capabilities->exposure_compensation = media::mojom::blink::Range::New();
  photo_capabilities->exposure_time = media::mojom::blink::Range::New();
  photo_capabilities->color_temperature = media::mojom::blink::Range::New();
  photo_capabilities->iso = media::mojom::blink::Range::New();
  photo_capabilities->brightness = media::mojom::blink::Range::New();
  photo_capabilities->contrast = media::mojom::blink::Range::New();
  photo_capabilities->saturation = media::mojom::blink::Range::New();
  photo_capabilities->sharpness = media::mojom::blink::Range::New();
  photo_capabilities->pan = media::mojom::blink::Range::New();
  photo_capabilities->tilt = media::mojom::blink::Range::New();
  photo_capabilities->zoom = media::mojom::blink::Range::New();
  photo_capabilities->focus_distance = media::mojom::blink::Range::New();
  photo_capabilities->supports_torch = false;
  photo_capabilities->red_eye_reduction =
      media::mojom::blink::RedEyeReduction::NEVER;
  photo_capabilities->supported_background_blur_modes = {};
  photo_capabilities->supported_eye_gaze_correction_modes = {};
  photo_capabilities->supported_face_framing_modes = {};
  photo_capabilities->supported_background_segmentation_mask_states = {};
  std::move(callback).Run(std::move(photo_capabilities));
}

}  // namespace blink
