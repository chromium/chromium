// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_GRAPHICS_3D_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_GRAPHICS_3D_SHARED_H_

#include <stdint.h>

#include <memory>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class CommandBuffer;
class GpuControl;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu.

namespace ppapi {

struct Graphics3DContextAttribs {
  gfx::Size offscreen_framebuffer_size;
  // -1 if invalid or unspecified.
  int32_t alpha_size = -1;
  int32_t depth_size = -1;
  int32_t stencil_size = -1;
  int32_t samples = -1;
  int32_t sample_buffers = -1;
  bool buffer_preserved = true;
  bool single_buffer = false;
};

class PPAPI_SHARED_EXPORT PPB_Graphics3D_Shared
    : public Resource,
      public thunk::PPB_Graphics3D_API {
 public:
  PPB_Graphics3D_Shared(const PPB_Graphics3D_Shared&) = delete;
  PPB_Graphics3D_Shared& operator=(const PPB_Graphics3D_Shared&) = delete;

  // Resource overrides.
  thunk::PPB_Graphics3D_API* AsPPB_Graphics3D_API() override;

  // PPB_Graphics3D_API implementation.
  int32_t GetAttribs(int32_t attrib_list[]) override;
  int32_t SetAttribs(const int32_t attrib_list[]) override;
  int32_t GetError() override;
  int32_t ResizeBuffers(int32_t width, int32_t height) override;
  int32_t SwapBuffers(scoped_refptr<TrackedCallback> callback) override;
  int32_t SwapBuffersWithSyncToken(scoped_refptr<TrackedCallback> callback,
                                   const gpu::SyncToken& sync_token,
                                   const gfx::Size& size) override;
  int32_t GetAttribMaxValue(int32_t attribute, int32_t* value) override;

  void* MapTexSubImage2DCHROMIUM(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 GLenum access) override;
  void UnmapTexSubImage2DCHROMIUM(const void* mem) override;

  gpu::gles2::GLES2Implementation* gles2_impl() { return gles2_impl_.get(); }
  gpu::gles2::GLES2Interface* gles2_interface();

  // Sends swap-buffers notification to the plugin.
  void SwapBuffersACK(int32_t pp_error);

 protected:
  explicit PPB_Graphics3D_Shared(PP_Instance instance);
  PPB_Graphics3D_Shared(const HostResource& host_resource,
                        const gfx::Size& size);
  ~PPB_Graphics3D_Shared() override;

  virtual gpu::CommandBuffer* GetCommandBuffer() = 0;
  virtual gpu::GpuControl* GetGpuControl() = 0;
  virtual int32_t DoSwapBuffers(const gpu::SyncToken& sync_token,
                                const gfx::Size& size) = 0;
  virtual void DoResize(gfx::Size size) = 0;

  bool HasPendingSwap() const;
  bool CreateGLES2Impl(gpu::gles2::GLES2Implementation* share_gles2);
  void DestroyGLES2Impl();

 private:
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  // A local cache of the size of the viewport. This is only valid in plugin
  // resources.
  gfx::Size size_;

  // Callback that needs to be executed when swap-buffers is completed.
  scoped_refptr<TrackedCallback> swap_callback_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_GRAPHICS_3D_SHARED_H_
