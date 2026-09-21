// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_H_

#include <GLES2/gl2.h>

#include <cstdint>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace cc {
class ClientTransferCacheEntry;
class DisplayItemList;
class ImageProvider;
}  // namespace cc

namespace gfx {
class Rect;
class Vector2d;
class Vector2dF;
}  // namespace gfx

namespace gl {
enum class GpuPreference;
}

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _ClientGpuFence* ClientGpuFence;
extern "C" typedef const struct _GLcolorSpace* GLcolorSpace;

namespace gpu {
class ClientSharedImage;

namespace gles2 {

using CopySharedImageSyncCallback = base::OnceCallback<gpu::SyncToken()>;

// This class is the interface for all client side GL functions.
class GLES2Interface : public InterfaceBase {
 public:
  GLES2Interface() = default;
  virtual ~GLES2Interface() = default;

  // Returns true if it's possible to do a copy of a SharedImage to a GL texture
  // directly.
  virtual bool CanCopySharedImageDirectlyToGLTexture(
      bool is_opaque,
      ClientSharedImage* shared_image,
      uint32_t dst_target,
      uint32_t dst_internal_format,
      uint32_t dst_type,
      int32_t dst_level,
      SkAlphaType dst_alpha_type);

  // Copies the contents of |source_shared_image| to |dst_texture| of the
  // current context. Assumes that |dst_texture| has already been allocated (via
  // BindTexture and TexImage2D) to the size of |src_rect|.
  // Returns a callback that the client is expected to invoke when they have
  // finished the access. Invoking the callback ends access and returns a
  // SyncToken.
  virtual CopySharedImageSyncCallback CopySharedImageToGLTextureViaTextureCopy(
      const gfx::Rect& src_rect,
      ClientSharedImage* source_shared_image,
      const gpu::SyncToken& source_sync_token,
      uint32_t dst_target,
      uint32_t dst_texture,
      uint32_t dst_internal_format,
      uint32_t dst_format,
      uint32_t dst_type,
      int32_t dst_level,
      SkAlphaType dst_alpha_type,
      GrSurfaceOrigin dst_origin);

  // Copies the contents of |source_shared_image| to |dst_texture| of the
  // current context. Assumes that |dst_texture| has already been allocated (via
  // BindTexture and TexImage2D) to the size of |src_rect|.
  // Returns a callback that the client is expected to invoke when they have
  // finished the access. Invoking the callback ends access and returns a
  // SyncToken.
  virtual CopySharedImageSyncCallback CopySharedImageDirectlyToGLTexture(
      const gfx::Rect& src_rect,
      ClientSharedImage* source_shared_image,
      const gpu::SyncToken& source_sync_token,
      bool is_opaque,
      uint32_t dst_target,
      uint32_t dst_texture,
      uint32_t dst_internal_format,
      uint32_t dst_format,
      uint32_t dst_type,
      int32_t dst_level,
      SkAlphaType dst_alpha_type,
      GrSurfaceOrigin dst_origin);

  virtual void FreeSharedMemory(void*) {}

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_interface_autogen.h"
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_H_
