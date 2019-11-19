// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_

#include <GLES2/gl2.h>
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace cc {
class DisplayItemList;
class ImageProvider;
}  // namespace cc

namespace gfx {
class ColorSpace;
class Rect;
class Size;
class Vector2dF;
enum class BufferUsage;
}  // namespace gfx

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _GLColorSpace* GLColorSpace;

namespace gpu {

struct Mailbox;

namespace raster {

enum RasterTexStorageFlags { kNone = 0, kOverlay = (1 << 0) };

class RasterInterface : public InterfaceBase {
 public:
  RasterInterface() {}
  virtual ~RasterInterface() {}

  virtual void CopySubTexture(const gpu::Mailbox& source_mailbox,
                              const gpu::Mailbox& dest_mailbox,
                              GLenum dest_target,
                              GLint xoffset,
                              GLint yoffset,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height) = 0;
  // OOP-Raster
  virtual void BeginRasterCHROMIUM(GLuint sk_color,
                                   GLuint msaa_sample_count,
                                   GLboolean can_use_lcd_text,
                                   const gfx::ColorSpace& color_space,
                                   const GLbyte* mailbox) = 0;

  // Heuristic decided on UMA data. This covers 85% of the cases where we need
  // to serialize ops > 512k.
  static constexpr size_t kDefaultMaxOpSizeHint = 600 * 1024;
  virtual void RasterCHROMIUM(const cc::DisplayItemList* list,
                              cc::ImageProvider* provider,
                              const gfx::Size& content_size,
                              const gfx::Rect& full_raster_rect,
                              const gfx::Rect& playback_rect,
                              const gfx::Vector2dF& post_translate,
                              GLfloat post_scale,
                              bool requires_clear,
                              size_t* max_op_size_hint) = 0;

  // Schedules a hardware-accelerated image decode and a sync token that's
  // released when the image decode is complete. If the decode could not be
  // scheduled, an empty sync token is returned. This method should only be
  // called if ContextSupport::CanDecodeWithHardwareAcceleration() returns true.
  virtual SyncToken ScheduleImageDecode(
      base::span<const uint8_t> encoded_data,
      const gfx::Size& output_size,
      uint32_t transfer_cache_entry_id,
      const gfx::ColorSpace& target_color_space,
      bool needs_mips) = 0;

  // Raster via GrContext.
  virtual GLuint CreateAndConsumeForGpuRaster(const gpu::Mailbox& mailbox) = 0;
  virtual void DeleteGpuRasterTexture(GLuint texture) = 0;
  virtual void BeginGpuRaster() = 0;
  virtual void EndGpuRaster() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_interface_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
