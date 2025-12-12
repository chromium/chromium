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

#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

InterpolationQuality ComputeInterpolationQuality(const gfx::SizeF& src,
                                                 const gfx::SizeF& dest,
                                                 bool is_data_complete) {
  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  const gfx::SizeF diff(std::abs(dest.width() - src.width()),
                        std::abs(dest.height() - src.height()));
  const bool width_nearly_equal =
      diff.width() < std::numeric_limits<float>::epsilon();
  const bool height_nearly_equal =
      diff.height() < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (width_nearly_equal && height_nearly_equal)
    return kInterpolationNone;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  static constexpr int kSmallImageSizeThreshold = 8;
  if (src.width() <= kSmallImageSizeThreshold ||
      src.height() <= kSmallImageSizeThreshold ||
      dest.width() <= kSmallImageSizeThreshold ||
      dest.height() <= kSmallImageSizeThreshold) {
    // Small image detected.

    auto nearly_integral = [](float value) {
      return std::abs(value - std::floor(value)) <
             std::numeric_limits<float>::epsilon();
    };

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!nearly_integral(dest.width()) &&
         src.width() > 1 + std::numeric_limits<float>::epsilon()) ||
        (!nearly_integral(dest.height()) &&
         src.height() > 1 + std::numeric_limits<float>::epsilon())) {
      return kInterpolationLow;
    }

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return kInterpolationNone;
  }

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  static constexpr float kLargeStretch = 3.0f;
  if (src.height() * kLargeStretch <= dest.height() ||
      src.width() * kLargeStretch <= dest.width()) {
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

  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  static constexpr float kFractionalChangeThreshold = 0.025f;
  if ((diff.width() / src.width() < kFractionalChangeThreshold) &&
      (diff.height() / src.height() < kFractionalChangeThreshold)) {
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
