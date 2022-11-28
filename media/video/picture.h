// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A picture buffer that is composed of one or more GLES2 textures.
// This is the media-namespace equivalent of PP_PictureBuffer_Dev.
class MEDIA_EXPORT PictureBuffer {
 public:
  using TextureIds = std::vector<uint32_t>;
  using TextureSizes = std::vector<gfx::Size>;

  PictureBuffer(int32_t id, const gfx::Size& size);
  PictureBuffer(int32_t id,
                const gfx::Size& size,
                const TextureIds& client_texture_ids);
  PictureBuffer(int32_t id,
                const gfx::Size& size,
                const TextureIds& client_texture_ids,
                const TextureIds& service_texture_ids,
                uint32_t texture_target,
                VideoPixelFormat pixel_format);
  PictureBuffer(int32_t id,
                const gfx::Size& size,
                const TextureIds& client_texture_ids,
                const std::vector<gpu::Mailbox>& texture_mailboxes,
                uint32_t texture_target,
                VideoPixelFormat pixel_format);
  PictureBuffer(int32_t id,
                const gfx::Size& size,
                const TextureSizes& texture_sizes,
                const TextureIds& client_texture_ids,
                const TextureIds& service_texture_ids,
                uint32_t texture_target,
                VideoPixelFormat pixel_format);
  PictureBuffer(const PictureBuffer& other);
  ~PictureBuffer();

  // Returns the client-specified id of the buffer.
  int32_t id() const { return id_; }

  // Returns the size of the buffer.
  gfx::Size size() const { return size_; }

  void set_size(const gfx::Size& size) { size_ = size; }

  // The client texture ids, i.e., those returned by Chrome's GL service.
  const TextureIds& client_texture_ids() const { return client_texture_ids_; }

  // The service texture ids, i.e., the real platform ids corresponding to
  // |client_texture_ids|.
  const TextureIds& service_texture_ids() const { return service_texture_ids_; }

  uint32_t texture_target() const { return texture_target_; }

  VideoPixelFormat pixel_format() const { return pixel_format_; }

  gfx::Size texture_size(size_t plane) const;

 private:
  int32_t id_;
  gfx::Size size_;
  TextureSizes texture_sizes_;
  TextureIds client_texture_ids_;
  TextureIds service_texture_ids_;
  std::vector<gpu::Mailbox> texture_mailboxes_;
  uint32_t texture_target_ = 0;
  VideoPixelFormat pixel_format_ = PIXEL_FORMAT_UNKNOWN;
};

// A decoded picture frame.
// This is the media-namespace equivalent of PP_Picture_Dev.
class MEDIA_EXPORT Picture {
 public:
  // An object that keeps alive a SharedImage until it goes out of scope.
  // Used to manage the lifetime of SharedImage-backed decoded frames.
  class MEDIA_EXPORT ScopedSharedImage
      : public base::RefCountedThreadSafe<ScopedSharedImage> {
   public:
    ScopedSharedImage(gpu::Mailbox mailbox,
                      uint32_t texture_target,
                      base::OnceClosure destruction_closure);
    const gpu::MailboxHolder& GetMailboxHolder() const {
      return mailbox_holder_;
    }

   private:
    friend class base::RefCountedThreadSafe<ScopedSharedImage>;
    ~ScopedSharedImage();

    base::OnceClosure destruction_closure_;
    gpu::MailboxHolder mailbox_holder_;
  };

  // Defaults |size_changed_| to false. Size changed is currently only used
  // by AVDA and is set via set_size_changd().
  Picture(int32_t picture_buffer_id,
          int32_t bitstream_buffer_id,
          const gfx::Rect& visible_rect,
          const gfx::ColorSpace& color_space,
          bool allow_overlay);
  Picture(const Picture&);
  ~Picture();

  // Returns the id of the picture buffer where this picture is contained.
  int32_t picture_buffer_id() const { return picture_buffer_id_; }

  // Returns the id of the bitstream buffer from which this frame was decoded.
  int32_t bitstream_buffer_id() const { return bitstream_buffer_id_; }

  // Returns the color space of the picture.
  const gfx::ColorSpace& color_space() const { return color_space_; }
  const absl::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }

  void set_hdr_metadata(const absl::optional<gfx::HDRMetadata>& hdr_metadata) {
    hdr_metadata_ = hdr_metadata;
  }

  // Returns the visible rectangle of the picture. Its size may be smaller
  // than the size of the PictureBuffer, as it is the only visible part of the
  // Picture contained in the PictureBuffer.
  gfx::Rect visible_rect() const { return visible_rect_; }

  bool allow_overlay() const { return allow_overlay_; }

  bool read_lock_fences_enabled() const { return read_lock_fences_enabled_; }

  void set_read_lock_fences_enabled(bool read_lock_fences_enabled) {
    read_lock_fences_enabled_ = read_lock_fences_enabled;
  }

  // Returns true when the VDA has adjusted the resolution of this Picture
  // without requesting new PictureBuffers. GpuVideoDecoder should read this
  // as a signal to update the size of the corresponding PicutreBuffer using
  // visible_rect() upon receiving this Picture from a VDA.
  bool size_changed() const { return size_changed_; }

  void set_size_changed(bool size_changed) { size_changed_ = size_changed; }

  bool texture_owner() const { return texture_owner_; }

  void set_texture_owner(bool texture_owner) { texture_owner_ = texture_owner; }

  bool wants_promotion_hint() const { return wants_promotion_hint_; }

  void set_wants_promotion_hint(bool wants_promotion_hint) {
    wants_promotion_hint_ = wants_promotion_hint;
  }

  void set_scoped_shared_image(
      scoped_refptr<ScopedSharedImage> scoped_shared_image,
      uint32_t plane = 0) {
    DCHECK(plane < scoped_shared_images_.size());
    scoped_shared_images_[plane] = scoped_shared_image;
  }

  scoped_refptr<ScopedSharedImage> scoped_shared_image(
      uint32_t plane = 0) const {
    DCHECK(plane < scoped_shared_images_.size());
    return scoped_shared_images_[plane];
  }

  void set_is_webgpu_compatible(bool is_webgpu_compatible) {
    is_webgpu_compatible_ = is_webgpu_compatible;
  }

  bool is_webgpu_compatible() { return is_webgpu_compatible_; }

 private:
  int32_t picture_buffer_id_;
  int32_t bitstream_buffer_id_;
  gfx::Rect visible_rect_;
  gfx::ColorSpace color_space_;
  absl::optional<gfx::HDRMetadata> hdr_metadata_;
  bool allow_overlay_;
  bool read_lock_fences_enabled_;
  bool size_changed_;
  bool texture_owner_;
  bool wants_promotion_hint_;
  bool is_webgpu_compatible_;
  std::array<scoped_refptr<ScopedSharedImage>, VideoFrame::kMaxPlanes>
      scoped_shared_images_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_PICTURE_H_
