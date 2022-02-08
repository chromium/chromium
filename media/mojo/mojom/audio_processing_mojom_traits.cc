// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_processing_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::AudioProcessingSettingsDataView,
                  media::AudioProcessingSettings>::
    Read(media::mojom::AudioProcessingSettingsDataView input,
         media::AudioProcessingSettings* out_settings) {
  *out_settings = media::AudioProcessingSettings();
  out_settings->echo_cancellation = input.echo_cancellation();
  out_settings->noise_suppression = input.noise_suppression();
  out_settings->transient_noise_suppression =
      input.transient_noise_suppression();
  out_settings->automatic_gain_control = input.automatic_gain_control();
  out_settings->experimental_automatic_gain_control =
      input.experimental_automatic_gain_control();
  out_settings->high_pass_filter = input.high_pass_filter();
  out_settings->multi_channel_capture_processing =
      input.multi_channel_capture_processing();
  out_settings->stereo_mirroring = input.stereo_mirroring();
  out_settings->force_apm_creation = input.force_apm_creation();
  return true;
}

}  // namespace mojo
