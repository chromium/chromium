// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_processing_mojom_traits.h"

namespace mojo {
namespace {
// Deserializes has_field and field into a std::optional.
#define DESERIALIZE_INTO_OPT(field) \
  if (input.has_##field())          \
  out_stats->field = input.field()
}  // namespace

// static
bool StructTraits<media::mojom::AudioProcessingStatsDataView,
                  media::AudioProcessingStats>::
    Read(media::mojom::AudioProcessingStatsDataView input,
         media::AudioProcessingStats* out_stats) {
  DESERIALIZE_INTO_OPT(echo_return_loss);
  DESERIALIZE_INTO_OPT(echo_return_loss_enhancement);
  return true;
}

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
  out_settings->high_pass_filter = input.high_pass_filter();
  out_settings->multi_channel_capture_processing =
      input.multi_channel_capture_processing();
  out_settings->stereo_mirroring = input.stereo_mirroring();
  out_settings->force_apm_creation = input.force_apm_creation();
  return true;
}

}  // namespace mojo
