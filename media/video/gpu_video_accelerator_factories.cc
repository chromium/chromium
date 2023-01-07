// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/gpu_video_accelerator_factories.h"

namespace media {

GpuVideoAcceleratorFactories::Supported
GpuVideoAcceleratorFactories::IsDecoderConfigSupportedOrUnknown(
    const VideoDecoderConfig& config) {
  if (!IsDecoderSupportKnown())
    return Supported::kUnknown;
  return IsDecoderConfigSupported(config);
}

}  // namespace media
