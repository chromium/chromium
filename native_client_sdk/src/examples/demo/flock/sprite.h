// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_DEMO_FLOCK_SPRITE_H_
#define EXAMPLES_DEMO_FLOCK_SPRITE_H_

#include <vector>
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"


// A Sprite is a simple container of a pixel buffer.  It knows how to
// composite itself to another pixel buffer of the same format.
class Sprite {
 public:
  // Initialize a Sprite to use the attached pixel buffer.  The Sprite takes
  // ownership of the pixel buffer, deleting it in the dtor.  The pixel
  // buffer is assumed to be 32-bit ARGB-8-8-8-8 pixel format, with pre-
  // multiplied alpha.  If |row_bytes| is 0, then the number of bytes per row
  // is assumed to be size.width() * sizeof(uint32_t).
  Sprite(uint32_t* pixel_buffer, const pp::Size& size, int32_t stride = 0);

  // Delete the pixel buffer.  It is assumed that the pixel buffer was created
  // using malloc().
  ~Sprite();

  // Reset the internal pixel buffer to a new one.  Deletes the old pixel
  // buffer.  Sprite takes ownership of the new pixel buffer.  If |row_bytes|
  // is 0, then the number of bytes per row is assumed to be size.width() *
  // sizeof(uint32_t).
  void SetPixelBuffer(uint32_t* pixel_buffer,
                      const pp::Size& size,
                      int32_t row_bytes);

  // Composite the section of the Sprite contained in |src_rect| into the given
  // pixel buffer at |dest_point|.  Performs an average of the source and
  // dest pixel, and all necessary clipping.
  void CompositeFromRectToPoint(const pp::Rect& src_rect,
                                uint32_t* dest_pixel_buffer,
                                const pp::Rect& dest_bounds,
                                int32_t dest_row_bytes,
                                const pp::Point& dest_point) const;

  // Accessors.
  const pp::Size& size() const {
    return pixel_buffer_size_;
  }

 private:
  uint32_t* pixel_buffer_;
  pp::Size pixel_buffer_size_;
  int32_t row_bytes_;
};

#endif  // EXAMPLES_DEMO_FLOCK_SPRITE_H_
