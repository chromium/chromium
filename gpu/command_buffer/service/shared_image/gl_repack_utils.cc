// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"

#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
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
  CopyImagePlane(src_data, src_stride, dst_data.data(), dst_stride,
                 src_pixmap.info().minRowBytes(), size.height());

  return dst_data;
}

void UnpackPixelDataWithStride(const gfx::Size& size,
                               const std::vector<uint8_t>& src_data,
                               size_t src_stride,
                               const SkPixmap& dst_pixmap) {
  uint8_t* dst_data = static_cast<uint8_t*>(dst_pixmap.writable_addr());
  size_t dst_stride = dst_pixmap.rowBytes();

  DCHECK_GT(dst_stride, src_stride);

  CopyImagePlane(src_data.data(), src_stride, dst_data, dst_stride,
                 dst_pixmap.info().minRowBytes(), size.height());
}

void SwizzleRedAndBlue(const SkPixmap& pixmap) {
  DCHECK_EQ(pixmap.info().bytesPerPixel(), 4);

  uint8_t* data = static_cast<uint8_t*>(pixmap.writable_addr());
  size_t stride = pixmap.rowBytes();

  for (int y = 0; y < pixmap.height(); ++y) {
    size_t row_offset = y * stride;
    for (int x = 0; x < pixmap.width(); ++x) {
      size_t pixel_offset = row_offset + x * 4;
      std::swap(data[pixel_offset], data[pixel_offset + 2]);
    }
  }
}

}  // namespace gpu
