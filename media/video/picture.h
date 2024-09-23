// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// A PictureBuffer carries metadata about a particular buffer tracked by the
// client of a VideoDecodeAccelerator.
class MEDIA_EXPORT PictureBuffer {
 public:
  explicit PictureBuffer(int32_t id);
  PictureBuffer(const PictureBuffer& other);
  ~PictureBuffer();

  // Returns the client-specified id of the buffer.
  int32_t id() const { return id_; }
 private:
  int32_t id_;
};

// A decoded picture frame.
// This is the media-namespace equivalent of PP_Picture_Dev.
class MEDIA_EXPORT Picture {
 public:
  Picture(int32_t picture_buffer_id,
          int32_t bitstream_buffer_id,
          const gfx::Rect& visible_rect);
  Picture(const Picture&);
  ~Picture();

  // Returns the id of the picture buffer where this picture is contained.
  int32_t picture_buffer_id() const { return picture_buffer_id_; }

  // Returns the id of the bitstream buffer from which this frame was decoded.
  int32_t bitstream_buffer_id() const { return bitstream_buffer_id_; }

  // Returns the visible rectangle of the picture. Its size may be smaller
  // than the size of the PictureBuffer, as it is the only visible part of the
  // Picture contained in the PictureBuffer.
  gfx::Rect visible_rect() const { return visible_rect_; }

 private:
  int32_t picture_buffer_id_;
  int32_t bitstream_buffer_id_;
  gfx::Rect visible_rect_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_PICTURE_H_
