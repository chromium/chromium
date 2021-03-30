// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/gpu_video_accelerator_factories.h"

namespace media {

GpuVideoAcceleratorFactories::Supported
GpuVideoAcceleratorFactories::IsDecoderConfigSupported(
    const VideoDecoderConfig& config) {
  if (!IsDecoderSupportKnown())
    return Supported::kUnknown;

  static_assert(media::VideoDecoderImplementation::kAlternate ==
                    media::VideoDecoderImplementation::kMaxValue,
                "Keep the array below in sync.");
  VideoDecoderImplementation decoder_impls[] = {
      VideoDecoderImplementation::kDefault,
      VideoDecoderImplementation::kAlternate};
  Supported supported = Supported::kUnknown;
  for (const auto& impl : decoder_impls) {
    supported = IsDecoderConfigSupported(impl, config);
    DCHECK_NE(supported, Supported::kUnknown);
    if (supported == Supported::kTrue)
      break;
  }

  return supported;
}

}  // namespace media
