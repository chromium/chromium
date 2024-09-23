// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/mojom/image_capture_types.h"

namespace mojo {

media::mojom::PhotoStatePtr CreateEmptyPhotoState() {
  media::mojom::PhotoStatePtr photo_capabilities =
      media::mojom::PhotoState::New();
  photo_capabilities->height = media::mojom::Range::New();
  photo_capabilities->width = media::mojom::Range::New();
  photo_capabilities->exposure_compensation = media::mojom::Range::New();
  photo_capabilities->exposure_time = media::mojom::Range::New();
  photo_capabilities->color_temperature = media::mojom::Range::New();
  photo_capabilities->iso = media::mojom::Range::New();
  photo_capabilities->brightness = media::mojom::Range::New();
  photo_capabilities->contrast = media::mojom::Range::New();
  photo_capabilities->saturation = media::mojom::Range::New();
  photo_capabilities->sharpness = media::mojom::Range::New();
  photo_capabilities->pan = media::mojom::Range::New();
  photo_capabilities->tilt = media::mojom::Range::New();
  photo_capabilities->zoom = media::mojom::Range::New();
  photo_capabilities->focus_distance = media::mojom::Range::New();
  photo_capabilities->torch = false;
  photo_capabilities->red_eye_reduction = media::mojom::RedEyeReduction::NEVER;
  photo_capabilities->supported_background_blur_modes = {};
  photo_capabilities->supported_eye_gaze_correction_modes = {};
  photo_capabilities->supported_face_framing_modes = {};
  photo_capabilities->supported_background_segmentation_mask_states = {};
  return photo_capabilities;
}

}  // namespace mojo
