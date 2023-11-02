// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "sprite.h"

namespace {

inline uint32_t Blend(uint32_t src1, uint32_t src2) {
  // Divide both sources by 2, then add them together using a mask
  // to avoid overflow.
  src1 = (src1 >> 1) & 0x7F7F7F7F;
  src2 = (src2 >> 1) & 0x7F7F7F7F;
  return src1 + src2;
}

}  // namespace


Sprite::Sprite(uint32_t* pixel_buffer,
               const pp::Size& size,
               int32_t row_bytes) {
  SetPixelBuffer(pixel_buffer, size, row_bytes);
}

Sprite::~Sprite() {
  delete[] pixel_buffer_;
}

void Sprite::SetPixelBuffer(uint32_t* pixel_buffer,
                            const pp::Size& size,
                            int32_t row_bytes) {
  pixel_buffer_ = pixel_buffer;
  pixel_buffer_size_ = size;
  row_bytes_ = row_bytes ? row_bytes : size.width() * sizeof(uint32_t);
}

void Sprite::CompositeFromRectToPoint(const pp::Rect& src_rect,
                                      uint32_t* dest_pixel_buffer,
                                      const pp::Rect& dest_bounds,
                                      int32_t dest_row_bytes,
                                      const pp::Point& dest_point) const {
  // Clip the source rect to the source image bounds.
  pp::Rect src_bounds(pp::Point(), size());
  pp::Rect src_rect_clipped(src_rect.Intersect(src_bounds));
  if (src_rect_clipped.IsEmpty())
    return;

  // Create a clipped rect in the destination coordinate space that contains the
  // final image.
  pp::Rect draw_rect(dest_point, src_rect_clipped.size());
  pp::Rect draw_rect_clipped(dest_bounds.Intersect(draw_rect));
  if (draw_rect_clipped.IsEmpty())
    return;
  // Transform the dest rect to the source image coordinate system.
  pp::Point src_offset(draw_rect_clipped.point());
  src_offset -= dest_point;
  src_rect_clipped.Offset(src_offset);
  src_rect_clipped.set_size(draw_rect_clipped.size());
  size_t src_byte_offset = src_rect_clipped.x() * sizeof(uint32_t) +
                           src_rect_clipped.y() * row_bytes_;
  const uint8_t* src_pixels =
      reinterpret_cast<const uint8_t*>(pixel_buffer_) + src_byte_offset;

  if (dest_row_bytes == 0)
    dest_row_bytes = dest_bounds.width() * sizeof(uint32_t);
  size_t dest_byte_offset = draw_rect_clipped.point().x() * sizeof(uint32_t) +
                            draw_rect_clipped.point().y() * dest_row_bytes;
  uint8_t* dest_pixels = reinterpret_cast<uint8_t*>(dest_pixel_buffer) +
                         dest_byte_offset;

  for (int32_t y = 0; y < src_rect_clipped.height(); ++y) {
    const uint32_t* src_scanline =
        reinterpret_cast<const uint32_t*>(src_pixels);
    uint32_t* dest_scanline = reinterpret_cast<uint32_t*>(dest_pixels);
    for (int32_t x = 0; x < src_rect_clipped.width(); ++x) {
      uint32_t src = *src_scanline++;
      uint32_t dst = *dest_scanline;
      *dest_scanline++ = Blend(dst, src);
    }
    src_pixels += row_bytes_;
    dest_pixels += dest_row_bytes;
  }
}

