// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"

#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gpu {

std::vector<uint8_t> RepackPixelDataAsRgb(const gfx::Size& size,
                                          const SkPixmap& src_pixmap,
                                          bool src_is_bgrx) {
  DCHECK_EQ(src_pixmap.info().bytesPerPixel(), 4);

  constexpr size_t kSrcBytesPerPixel = 4;
  constexpr size_t kDstBytesPerPixel = 3;

  const uint8_t* src_data = static_cast<const uint8_t*>(src_pixmap.addr());
  size_t src_stride = src_pixmap.rowBytes();

  // 3 bytes per pixel with 4 byte row alignment.
  size_t dst_stride =
      base::bits::AlignUp<size_t>(size.width() * kDstBytesPerPixel, 4);
  std::vector<uint8_t> dst_data(dst_stride * size.height());

  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      auto* src = &src_data[y * src_stride + x * kSrcBytesPerPixel];
      auto* dst = &dst_data[y * dst_stride + x * kDstBytesPerPixel];
      if (src_is_bgrx) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
      } else {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
      }
    }
  }

  return dst_data;
}

std::vector<uint8_t> RepackPixelDataWithStride(const gfx::Size& size,
                                               const SkPixmap& src_pixmap,
                                               size_t dst_stride) {
  const uint8_t* src_data = static_cast<const uint8_t*>(src_pixmap.addr());
  size_t src_stride = src_pixmap.rowBytes();

  DCHECK_LT(dst_stride, src_stride);

  std::vector<uint8_t> dst_data(dst_stride * size.height());
  for (int y = 0; y < size.height(); ++y) {
    memcpy(&dst_data[y * dst_stride], &src_data[y * src_stride], dst_stride);
  }
  return dst_data;
}

}  // namespace gpu
