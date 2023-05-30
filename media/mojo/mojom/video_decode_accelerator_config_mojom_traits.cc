// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_decode_accelerator_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::VideoDecodeAcceleratorConfigDataView,
                  media::VideoDecodeAccelerator::Config>::
    Read(media::mojom::VideoDecodeAcceleratorConfigDataView input,
         media::VideoDecodeAccelerator::Config* output) {
  if (!input.ReadProfile(&output->profile)) {
    return false;
  }
  if (!input.ReadEncryptionScheme(&output->encryption_scheme)) {
    return false;
  }
  if (!input.ReadCdmId(&output->cdm_id)) {
    return false;
  }
  output->is_deferred_initialization_allowed =
      input.is_deferred_initialization_allowed();
  if (!input.ReadOverlayInfo(&output->overlay_info)) {
    return false;
  }
  if (!input.ReadInitialExpectedCodedSize(
          &output->initial_expected_coded_size)) {
    return false;
  }
  if (!input.ReadSupportedOutputFormats(&output->supported_output_formats)) {
    return false;
  }
  if (!input.ReadSps(&output->sps)) {
    return false;
  }
  if (!input.ReadPps(&output->pps)) {
    return false;
  }
  if (!input.ReadContainerColorSpace(&output->container_color_space)) {
    return false;
  }
  if (!input.ReadTargetColorSpace(&output->target_color_space)) {
    return false;
  }
  if (!input.ReadHdrMetadata(&output->hdr_metadata)) {
    return false;
  }
  return true;
}

}  // namespace mojo
