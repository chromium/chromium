// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/picture.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace media {

PictureBuffer::PictureBuffer(int32_t id, const gfx::Size& size)
    : id_(id), size_(size) {}

PictureBuffer::PictureBuffer(int32_t id,
                             const gfx::Size& size,
                             uint32_t service_texture_id,
                             uint32_t texture_target,
                             VideoPixelFormat pixel_format)
    : id_(id),
      size_(size),
      service_texture_id_(service_texture_id),
      texture_target_(texture_target),
      pixel_format_(pixel_format) {}

PictureBuffer::PictureBuffer(const PictureBuffer& other) = default;

PictureBuffer::~PictureBuffer() = default;

Picture::Picture(int32_t picture_buffer_id,
                 int32_t bitstream_buffer_id,
                 const gfx::Rect& visible_rect,
                 const gfx::ColorSpace& color_space,
                 bool allow_overlay)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      visible_rect_(visible_rect),
      color_space_(color_space),
      allow_overlay_(allow_overlay),
      read_lock_fences_enabled_(false),
      size_changed_(false),
      texture_owner_(false),
      wants_promotion_hint_(false),
      is_webgpu_compatible_(false) {}

Picture::Picture(const Picture& other) = default;

Picture::~Picture() = default;

Picture::ScopedSharedImage::ScopedSharedImage(
    gpu::Mailbox mailbox,
    uint32_t texture_target,
    base::OnceClosure destruction_closure)
    : destruction_closure_(std::move(destruction_closure)),
      mailbox_holder_(mailbox, gpu::SyncToken(), texture_target) {}

Picture::ScopedSharedImage::~ScopedSharedImage() {
  std::move(destruction_closure_).Run();
}

}  // namespace media
