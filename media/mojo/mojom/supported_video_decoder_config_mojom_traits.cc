// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/supported_video_decoder_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::SupportedVideoDecoderConfigDataView,
                  media::SupportedVideoDecoderConfig>::
    Read(media::mojom::SupportedVideoDecoderConfigDataView input,
         media::SupportedVideoDecoderConfig* output) {
  if (!input.ReadProfileMin(&output->profile_min) ||
      output->profile_min == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }

  if (!input.ReadProfileMax(&output->profile_max) ||
      output->profile_max == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }

  if (output->profile_max < output->profile_min) {
    return false;
  }

  if (!input.ReadCodedSizeMin(&output->coded_size_min)) {
    return false;
  }

  if (!input.ReadCodedSizeMax(&output->coded_size_max)) {
    return false;
  }

  if (output->coded_size_max.IsEmpty() || output->coded_size_min.IsEmpty()) {
    return false;
  }

  if (output->coded_size_max.width() < output->coded_size_min.width() ||
      output->coded_size_max.height() < output->coded_size_min.height()) {
    return false;
  }

  if (input.require_encrypted() && !input.allow_encrypted()) {
    // Inconsistent information.
    return false;
  }

  output->allow_encrypted = input.allow_encrypted();
  output->require_encrypted = input.require_encrypted();

  return true;
}

}  // namespace mojo
