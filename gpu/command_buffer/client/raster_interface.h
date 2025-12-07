// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_

#include <GLES2/gl2.h>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/raster_cmd_enums.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace cc {
class DisplayItemList;
class ImageProvider;
struct ElementId;
}  // namespace cc

namespace gfx {
class ColorSpace;
class Point;
class PointF;
class Rect;
class Size;
class Vector2dF;
enum class BufferUsage;
}  // namespace gfx

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef const struct _GLcolorSpace* GLcolorSpace;

namespace gpu {

struct Mailbox;

namespace raster {

enum RasterTexStorageFlags { kNone = 0, kOverlay = (1 << 0) };

class RasterInterface : public InterfaceBase {
 public:
  RasterInterface() {}
  virtual ~RasterInterface() {}

  // This function will not perform any color conversion during the copy.
  // The same width/height is assumed for the destination.
  virtual void CopySharedImage(const gpu::Mailbox& source_mailbox,
                               const gpu::Mailbox& dest_mailbox,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height) = 0;
  // This function will not perform any color conversion during the copy.
  virtual void CopySharedImage(const gpu::Mailbox& source_mailbox,
                               const gpu::Mailbox& dest_mailbox,
                               const gfx::Rect& source_rect,
                               const gfx::Rect& dest_rect) = 0;

  // Asynchronously writes pixels from caller-owned memory inside
  // |src_sk_pixmap| into |dest_mailbox|.
  // NOTE: This is only for single planar shared images (RGB). For multiplanar
  // shared images, perform WritePixelsYUV.
  virtual void WritePixels(const gpu::Mailbox& dest_mailbox,
                           int dst_x_offset,
                           int dst_y_offset,
                           GLenum texture_target,
                           const SkPixmap& src_sk_pixmap) = 0;

  // Asynchronously writes YUV pixels from caller-owned memory inside
  // |src_yuv_pixmaps| into |dest_mailbox| for all planes. Should be used only
  // with YUV source images.
  // NOTE: This does not perform color space conversions and just uploads
  // pixels. For color space conversions (if needed), perform a CopySharedImage.
  virtual void WritePixelsYUV(const gpu::Mailbox& dest_mailbox,
                              const SkYUVAPixmaps& src_yuv_pixmap) = 0;

  // OOP-Raster

  // msaa_sample_count has no effect unless msaa_mode is set to kMSAA
  virtual void BeginRasterCHROMIUM(SkColor4f sk_color_4f,
                                   GLboolean needs_clear,
                                   GLuint msaa_sample_count,
                                   MsaaMode msaa_mode,
                                   GLboolean can_use_lcd_text,
                                   GLboolean visible,
                                   const gfx::ColorSpace& color_space,
                                   float hdr_headroom,
                                   const GLbyte* mailbox) = 0;

  // Heuristic decided on UMA data. This covers 85% of the cases where we need
  // to serialize ops > 512k.
  static constexpr size_t kDefaultMaxOpSizeHint = 600 * 1024;
  using ScrollOffsetMap = base::flat_map<cc::ElementId, gfx::PointF>;
  virtual void RasterCHROMIUM(
      const cc::DisplayItemList* list,
      cc::ImageProvider* provider,
      const gfx::Size& content_size,
      const gfx::Rect& full_raster_rect,
      const gfx::Rect& playback_rect,
      const gfx::Vector2dF& post_translate,
      const gfx::Vector2dF& post_scale,
      bool requires_clear,
      const ScrollOffsetMap* raster_inducing_scroll_offsets,
      size_t* max_op_size_hint) = 0;

  // Starts an asynchronous readback of `source_mailbox` into caller-owned
  // memory represented by `out`.
  // `dst_row_bytes` is a per row stride expected in the `out` buffer.
  // |source_origin| specifies texture coordinate directions, but
  // pixels in `out` are laid out with top-left origin.
  // Currently supports the kRGBA_8888_SkColorType and
  // kBGRA_8888_SkColorType color types.
  // The memory backing `out` must remain valid until `readback_done` is called
  // with a bool indicating if the readback was successful.
  // `source_size` describes dimensions of the `source_mailbox` texture.
  // `dst_info` and `source_starting_point` describe the subregion that needs
  // to be read.
  // On success, `out` will contain the pixel data copied back from the GPU
  // process.
  virtual void ReadbackARGBPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      GrSurfaceOrigin source_origin,
      const gfx::Size& source_size,
      const gfx::Point& source_starting_point,
      const SkImageInfo& dst_info,
      GLuint dst_row_bytes,
      base::span<uint8_t> out,
      base::OnceCallback<void(bool)> readback_done) = 0;

  // Starts an asynchronous readback and translation of RGBA `source_mailbox`
  // into caller-owned memory represented by `[yuv]_plane_data`. The memory
  // backing these spans must remain valid until `readback_done` is called with
  // a bool indicating if readback was successful. On success, the provided
  // spans will contain pixel data in I420 format copied from `source_mailbox`
  // in the GPU process. `release_mailbox` is called when all operations
  // requiring a valid mailbox have completed, indicating that the caller can
  // perform any necessary cleanup.
  virtual void ReadbackYUVPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      const gfx::Size& source_size,
      const gfx::Rect& output_rect,
      bool vertically_flip_texture,
      int y_plane_row_stride_bytes,
      base::span<uint8_t> y_plane_data,
      int u_plane_row_stride_bytes,
      base::span<uint8_t> u_plane_data,
      int v_plane_row_stride_bytes,
      base::span<uint8_t> v_plane_data,
      const gfx::Point& paste_location,
      base::OnceCallback<void()> release_mailbox,
      base::OnceCallback<void(bool)> readback_done) = 0;

  // Synchronously does a readback of SkImage pixels for given |plane_index|
  // from |source_mailbox| into caller-owned memory |dst_pixels|. |plane_index|
  // applies to multiplanar textures in mailboxes, for example YUV images
  // produced by the VideoDecoder. |plane_index| as 0 should be passed for known
  // single-plane textures.
  virtual bool ReadbackImagePixels(const gpu::Mailbox& source_mailbox,
                                   const SkImageInfo& dst_info,
                                   GLuint dst_row_bytes,
                                   int src_x,
                                   int src_y,
                                   int plane_index,
                                   void* dst_pixels) = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_interface_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
