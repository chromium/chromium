// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/picture.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace media {

PictureBuffer::PictureBuffer(int32_t id) : id_(id) {}

PictureBuffer::PictureBuffer(const PictureBuffer& other) = default;

PictureBuffer::~PictureBuffer() = default;

Picture::Picture(int32_t picture_buffer_id,
                 int32_t bitstream_buffer_id,
                 const gfx::Rect& visible_rect)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      visible_rect_(visible_rect) {}

Picture::Picture(const Picture& other) = default;

Picture::~Picture() = default;

}  // namespace media
