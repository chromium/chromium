// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/supported_video_decoder_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::SupportedVideoDecoderConfigDataView,
                  media::SupportedVideoDecoderConfig>::
    Read(media::mojom::SupportedVideoDecoderConfigDataView input,
         media::SupportedVideoDecoderConfig* output) {
  if (!input.ReadProfileMin(&output->profile_min))
    return false;

  if (!input.ReadProfileMax(&output->profile_max))
    return false;

  if (!input.ReadCodedSizeMin(&output->coded_size_min))
    return false;

  if (!input.ReadCodedSizeMax(&output->coded_size_max))
    return false;

  output->allow_encrypted = input.allow_encrypted();
  output->require_encrypted = input.require_encrypted();

  return true;
}

}  // namespace mojo
