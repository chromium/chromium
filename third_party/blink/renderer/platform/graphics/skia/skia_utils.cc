/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

#include <algorithm>
#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {

bool NearlyIntegral(float value) {
  return fabs(value - floorf(value)) < std::numeric_limits<float>::epsilon();
}

InterpolationQuality ComputeInterpolationQuality(float src_width,
                                                 float src_height,
                                                 float dest_width,
                                                 float dest_height,
                                                 bool is_data_complete) {
  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  const float kFractionalChangeThreshold = 0.025f;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  float diff_width = fabs(dest_width - src_width);
  float diff_height = fabs(dest_height - src_height);
  bool width_nearly_equal = diff_width < std::numeric_limits<float>::epsilon();
  bool height_nearly_equal =
      diff_height < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (width_nearly_equal && height_nearly_equal)
    return kInterpolationNone;

  if (src_width <= kSmallImageSizeThreshold ||
      src_height <= kSmallImageSizeThreshold ||
      dest_width <= kSmallImageSizeThreshold ||
      dest_height <= kSmallImageSizeThreshold) {
    // Small image detected.

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!NearlyIntegral(dest_width) &&
         src_width > 1 + std::numeric_limits<float>::epsilon()) ||
        (!NearlyIntegral(dest_height) &&
         src_height > 1 + std::numeric_limits<float>::epsilon()))
      return kInterpolationLow;

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return kInterpolationNone;
  }

  if (src_height * kLargeStretch <= dest_height ||
      src_width * kLargeStretch <= dest_width) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (width_nearly_equal || height_nearly_equal)
      return kInterpolationNone;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return kInterpolationLow;
  }

  if ((diff_width / src_width < kFractionalChangeThreshold) &&
      (diff_height / src_height < kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return kInterpolationNone;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!is_data_complete)
    return kInterpolationLow;

  // Everything else gets resampled at default quality.
  return GetDefaultInterpolationQuality();
}

SkColor ScaleAlpha(SkColor color, float alpha) {
  const auto clamped_alpha = std::max(0.0f, std::min(1.0f, alpha));
  const auto rounded_alpha =
      base::ClampRound<U8CPU>(SkColorGetA(color) * clamped_alpha);

  return SkColorSetA(color, rounded_alpha);
}

bool ApproximatelyEqualSkColorSpaces(sk_sp<SkColorSpace> src_color_space,
                                     sk_sp<SkColorSpace> dst_color_space) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (!src_color_space && !dst_color_space)
    return true;
  skcms_ICCProfile src_profile, dst_profile;
  src_color_space->toProfile(&src_profile);
  dst_color_space->toProfile(&dst_profile);
  return skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile);
}

sk_sp<SkData> TryAllocateSkData(size_t size) {
  void* buffer =
      Partitions::BufferPartition()
          ->AllocInline<partition_alloc::AllocFlags::kReturnNull |
                        partition_alloc::AllocFlags::kZeroFill>(size, "SkData");
  if (!buffer)
    return nullptr;
  return SkData::MakeWithProc(
      buffer, size,
      [](const void* buffer, void* context) {
        Partitions::BufferPartition()->Free(const_cast<void*>(buffer));
      },
      /*context=*/nullptr);
}

}  // namespace blink
