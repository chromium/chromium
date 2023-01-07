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
                             const TextureIds& client_texture_ids)
    : id_(id), size_(size), client_texture_ids_(client_texture_ids) {
  DCHECK(!client_texture_ids_.empty());
}

PictureBuffer::PictureBuffer(int32_t id,
                             const gfx::Size& size,
                             const TextureIds& client_texture_ids,
                             const TextureIds& service_texture_ids,
                             uint32_t texture_target,
                             VideoPixelFormat pixel_format)
    : id_(id),
      size_(size),
      client_texture_ids_(client_texture_ids),
      service_texture_ids_(service_texture_ids),
      texture_target_(texture_target),
      pixel_format_(pixel_format) {
  // We either not have client texture ids at all, or if we do, then their
  // number must be the same as the number of service texture ids.
  DCHECK(client_texture_ids_.empty() ||
         client_texture_ids_.size() == service_texture_ids_.size());
}

PictureBuffer::PictureBuffer(int32_t id,
                             const gfx::Size& size,
                             const TextureIds& client_texture_ids,
                             const std::vector<gpu::Mailbox>& texture_mailboxes,
                             uint32_t texture_target,
                             VideoPixelFormat pixel_format)
    : id_(id),
      size_(size),
      client_texture_ids_(client_texture_ids),
      texture_mailboxes_(texture_mailboxes),
      texture_target_(texture_target),
      pixel_format_(pixel_format) {
  DCHECK_EQ(client_texture_ids.size(), texture_mailboxes.size());
}

PictureBuffer::PictureBuffer(int32_t id,
                             const gfx::Size& size,
                             const TextureSizes& texture_sizes,
                             const TextureIds& client_texture_ids,
                             const TextureIds& service_texture_ids,
                             uint32_t texture_target,
                             VideoPixelFormat pixel_format)
    : id_(id),
      size_(size),
      texture_sizes_(texture_sizes),
      client_texture_ids_(client_texture_ids),
      service_texture_ids_(service_texture_ids),
      texture_target_(texture_target),
      pixel_format_(pixel_format) {
  // We either not have client texture ids at all, or if we do, then their
  // number must be the same as the number of service texture ids.
  DCHECK(client_texture_ids_.empty() ||
         client_texture_ids_.size() == service_texture_ids_.size());
}

PictureBuffer::PictureBuffer(const PictureBuffer& other) = default;

PictureBuffer::~PictureBuffer() = default;

gfx::Size PictureBuffer::texture_size(size_t plane) const {
  if (plane >= texture_sizes_.size()) {
    LOG(ERROR) << "Missing texture size for plane " << plane;
    return gfx::Size();
  }

  return texture_sizes_[plane];
}

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
