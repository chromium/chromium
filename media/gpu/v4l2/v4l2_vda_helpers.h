// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_
#define MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/gpu/chromeos/image_processor.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class V4L2Device;

// Helper static methods to be shared between V4L2VideoDecodeAccelerator and
// V4L2SliceVideoDecodeAccelerator. This avoids some code duplication between
// these very similar classes.
// Note: this namespace can be removed once the V4L2VDA is deprecated.
namespace v4l2_vda_helpers {

// Returns a usable input format of image processor. Return 0 if not found.
uint32_t FindImageProcessorInputFormat(V4L2Device* vda_device);
// Return a usable output format of image processor. Return 0 if not found.
uint32_t FindImageProcessorOutputFormat(V4L2Device* ip_device);

// Create and return an image processor for the given parameters, or nullptr
// if it cannot be created.
//
// |vda_output_format| is the output format of the VDA, i.e. the IP's input
// format.
// |ip_output_format| is the output format that the IP must produce.
// |vda_output_coded_size| is the coded size of the VDA output buffers (i.e.
// the input coded size for the IP).
// |ip_output_coded_size| is the coded size of the output buffers that the IP
// must produce.
// |visible_size| is the visible size of both the input and output buffers.
// |nb_buffers| is the exact number of output buffers that the IP must create.
// |image_processor_output_mode| specifies whether the IP must allocate its
// own buffers or rely on imported ones.
// |error_cb| is the error callback passed to V4L2ImageProcessor::Create().
std::unique_ptr<ImageProcessor> CreateImageProcessor(
    uint32_t vda_output_format,
    uint32_t ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Size& visible_size,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    ImageProcessor::ErrorCB error_cb);

}  // namespace v4l2_vda_helpers
}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_
