// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_factory.h"

#include <stddef.h>

#include "base/callback.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/libyuv_image_processor.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_processor.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC)

namespace media {

// static
std::unique_ptr<ImageProcessor> ImageProcessorFactory::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    size_t num_buffers,
    ImageProcessor::ErrorCB error_cb) {
  std::unique_ptr<ImageProcessor> image_processor;
#if BUILDFLAG(USE_VAAPI)
  image_processor = VaapiImageProcessor::Create(
      input_config, output_config, preferred_output_modes, error_cb);
  if (image_processor)
    return image_processor;
#endif  // BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(USE_V4L2_CODEC)
  for (auto output_mode : preferred_output_modes) {
    image_processor = V4L2ImageProcessor::Create(
        V4L2Device::Create(), input_config, output_config, output_mode,
        num_buffers, error_cb);
    if (image_processor)
      return image_processor;
  }
#endif  // BUILDFLAG(USE_V4L2_CODEC)
  for (auto output_mode : preferred_output_modes) {
    image_processor = LibYUVImageProcessor::Create(input_config, output_config,
                                                   output_mode, error_cb);
    if (image_processor)
      return image_processor;
  }
  return nullptr;
}

}  // namespace media
