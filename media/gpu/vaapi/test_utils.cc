// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test_utils.h"

#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_types.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {
namespace vaapi_test_utils {

std::string TestParamToString(
    const testing::TestParamInfo<TestParam>& param_info) {
  return param_info.param.test_name;
}

DecodedImage ScopedVAImageToDecodedImage(const ScopedVAImage* scoped_va_image) {
  DecodedImage decoded_image{};

  decoded_image.fourcc = scoped_va_image->image()->format.fourcc;
  decoded_image.number_of_planes = scoped_va_image->image()->num_planes;
  decoded_image.size =
      gfx::Size(base::strict_cast<int>(scoped_va_image->image()->width),
                base::strict_cast<int>(scoped_va_image->image()->height));

  DCHECK_LE(base::strict_cast<size_t>(decoded_image.number_of_planes),
            kMaxNumberPlanes);

  // This is safe because |number_of_planes| is retrieved from the VA-API and it
  // can not be greater than 3, which is also the size of the |planes| array.
  for (uint32_t i = 0u; i < decoded_image.number_of_planes; ++i) {
    decoded_image.planes[i].data =
        static_cast<uint8_t*>(scoped_va_image->va_buffer()->data()) +
        scoped_va_image->image()->offsets[i];
    decoded_image.planes[i].stride =
        base::checked_cast<int>(scoped_va_image->image()->pitches[i]);
  }

  return decoded_image;
}

bool CompareImages(const DecodedImage& reference_image,
                   const DecodedImage& hw_decoded_image,
                   double min_ssim) {
  if (reference_image.fourcc != VA_FOURCC_I420)
    return false;

  // Uses the reference image's size as the ground truth.
  const gfx::Size image_size = reference_image.size;
  if (image_size != hw_decoded_image.size) {
    DLOG(ERROR) << "Wrong expected software decoded image size, "
                << image_size.ToString() << " versus VaAPI provided "
                << hw_decoded_image.size.ToString();
    return false;
  }

  double ssim = 0;
  const uint32_t hw_fourcc = hw_decoded_image.fourcc;
  if (hw_fourcc == VA_FOURCC_I420) {
    ssim = libyuv::I420Ssim(
        reference_image.planes[0].data, reference_image.planes[0].stride,
        reference_image.planes[1].data, reference_image.planes[1].stride,
        reference_image.planes[2].data, reference_image.planes[2].stride,
        hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
        hw_decoded_image.planes[1].data, hw_decoded_image.planes[1].stride,
        hw_decoded_image.planes[2].data, hw_decoded_image.planes[2].stride,
        image_size.width(), image_size.height());
  } else if (hw_fourcc == VA_FOURCC_NV12 || hw_fourcc == VA_FOURCC_YUY2 ||
             hw_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
    // Calculate the stride for the chroma planes.
    const gfx::Size half_image_size((image_size.width() + 1) / 2,
                                    (image_size.height() + 1) / 2);
    // Temporary planes to hold intermediate conversions to I420 (i.e. NV12 to
    // I420 or YUYV/2 to I420).
    auto temp_y = std::make_unique<uint8_t[]>(image_size.GetArea());
    auto temp_u = std::make_unique<uint8_t[]>(half_image_size.GetArea());
    auto temp_v = std::make_unique<uint8_t[]>(half_image_size.GetArea());
    int conversion_result = -1;

    if (hw_fourcc == VA_FOURCC_NV12) {
      conversion_result = libyuv::NV12ToI420(
          hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
          hw_decoded_image.planes[1].data, hw_decoded_image.planes[1].stride,
          temp_y.get(), image_size.width(), temp_u.get(),
          half_image_size.width(), temp_v.get(), half_image_size.width(),
          image_size.width(), image_size.height());
    } else {
      // |hw_fourcc| is YUY2 or YUYV, which are handled the same.
      // TODO(crbug.com/868400): support other formats/planarities/pitches.
      conversion_result = libyuv::YUY2ToI420(
          hw_decoded_image.planes[0].data, hw_decoded_image.planes[0].stride,
          temp_y.get(), image_size.width(), temp_u.get(),
          half_image_size.width(), temp_v.get(), half_image_size.width(),
          image_size.width(), image_size.height());
    }
    if (conversion_result != 0) {
      DLOG(ERROR) << "libyuv conversion error";
      return false;
    }

    ssim = libyuv::I420Ssim(
        reference_image.planes[0].data, reference_image.planes[0].stride,
        reference_image.planes[1].data, reference_image.planes[1].stride,
        reference_image.planes[2].data, reference_image.planes[2].stride,
        temp_y.get(), image_size.width(), temp_u.get(), half_image_size.width(),
        temp_v.get(), half_image_size.width(), image_size.width(),
        image_size.height());
  } else {
    DLOG(ERROR) << "HW FourCC not supported: " << FourccToString(hw_fourcc);
    return false;
  }

  if (ssim < min_ssim) {
    DLOG(ERROR) << "SSIM too low: " << ssim << " < " << min_ssim;
    return false;
  }

  return true;
}

}  // namespace vaapi_test_utils
}  // namespace media
