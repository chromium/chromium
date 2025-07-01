// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_
#define PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace gfx {
class Size;
}

namespace gpu {
struct SyncToken;
}

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Graphics3D_API {
 public:
  virtual ~PPB_Graphics3D_API() {}

  // Graphics3D API.
  virtual int32_t GetAttribs(int32_t attrib_list[]) = 0;
  virtual int32_t SetAttribs(const int32_t attrib_list[]) = 0;
  virtual int32_t GetError() = 0;
  virtual int32_t ResizeBuffers(int32_t width, int32_t height) = 0;
  virtual int32_t SwapBuffers(scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t SwapBuffersWithSyncToken(
      scoped_refptr<TrackedCallback> callback,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size) = 0;
  virtual int32_t GetAttribMaxValue(int32_t attribute, int32_t* value) = 0;

  // Graphics3DTrusted API.
  virtual PP_Bool SetGetBuffer(int32_t shm_id) = 0;
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                          int32_t* id) = 0;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) = 0;
  virtual PP_Bool Flush(int32_t put_offset, uint64_t release_count) = 0;
  virtual gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                        int32_t end) = 0;
  virtual gpu::CommandBuffer::State WaitForGetOffsetInRange(
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end) = 0;

  // GLESChromiumTextureMapping.
  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) = 0;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) = 0;

  virtual void EnsureWorkVisible() = 0;
  virtual void ResolveAndDetachFramebuffer() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_
