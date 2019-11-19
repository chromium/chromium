// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vda_helpers.h"

#include "media/base/color_plane_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"

namespace media {
namespace v4l2_vda_helpers {

uint32_t FindImageProcessorInputFormat(V4L2Device* vda_device) {
  std::vector<uint32_t> processor_input_formats =
      V4L2ImageProcessor::GetSupportedInputFormats();

  struct v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (vda_device->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (std::find(processor_input_formats.begin(),
                  processor_input_formats.end(),
                  fmtdesc.pixelformat) != processor_input_formats.end()) {
      DVLOGF(3) << "Image processor input format=" << fmtdesc.description;
      return fmtdesc.pixelformat;
    }
    ++fmtdesc.index;
  }
  return 0;
}

uint32_t FindImageProcessorOutputFormat(V4L2Device* ip_device) {
  // Prefer YVU420 and NV12 because ArcGpuVideoDecodeAccelerator only supports
  // single physical plane.
  static constexpr uint32_t kPreferredFormats[] = {V4L2_PIX_FMT_NV12,
                                                   V4L2_PIX_FMT_YVU420};
  auto preferred_formats_first = [](uint32_t a, uint32_t b) -> bool {
    auto* iter_a = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), a);
    auto* iter_b = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), b);
    return iter_a < iter_b;
  };

  std::vector<uint32_t> processor_output_formats =
      V4L2ImageProcessor::GetSupportedOutputFormats();

  // Move the preferred formats to the front.
  std::sort(processor_output_formats.begin(), processor_output_formats.end(),
            preferred_formats_first);

  for (uint32_t processor_output_format : processor_output_formats) {
    if (ip_device->CanCreateEGLImageFrom(processor_output_format)) {
      DVLOGF(3) << "Image processor output format=" << processor_output_format;
      return processor_output_format;
    }
  }

  return 0;
}

std::unique_ptr<ImageProcessor> CreateImageProcessor(
    uint32_t vda_output_format,
    uint32_t ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Size& visible_size,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    ImageProcessor::ErrorCB error_cb) {
  // TODO(crbug.com/917798): Use ImageProcessorFactory::Create() once we remove
  //     |image_processor_device_| from V4L2VideoDecodeAccelerator.
  auto image_processor = V4L2ImageProcessor::Create(
      image_processor_device,
      ImageProcessor::PortConfig(Fourcc::FromV4L2PixFmt(vda_output_format),
                                 vda_output_coded_size, {}, visible_size,
                                 {VideoFrame::STORAGE_DMABUFS}),
      ImageProcessor::PortConfig(Fourcc::FromV4L2PixFmt(ip_output_format),
                                 ip_output_coded_size, {}, visible_size,
                                 {VideoFrame::STORAGE_DMABUFS}),
      image_processor_output_mode, nb_buffers, std::move(error_cb));
  if (!image_processor)
    return nullptr;

  if (image_processor->output_config().size != ip_output_coded_size) {
    VLOGF(1) << "Image processor should be able to use the requested output "
             << "coded size " << ip_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->output_config().size.ToString();
    return nullptr;
  }

  if (image_processor->input_config().size != vda_output_coded_size) {
    VLOGF(1) << "Image processor should be able to take the output coded "
             << "size of decoder " << vda_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->input_config().size.ToString();
    return nullptr;
  }

  return image_processor;
}

}  // namespace v4l2_vda_helpers
}  // namespace media
