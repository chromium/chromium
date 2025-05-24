// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/resource.h"

namespace gpu {
struct Capabilities;
struct GLCapabilities;
class CommandBufferProxyImpl;
class ClientSharedImageInterface;
}

namespace content {

class PPB_Graphics3D_Impl : public ppapi::PPB_Graphics3D_Shared,
                            public gpu::GpuControlClient {
 public:
  static PP_Resource CreateRaw(
      PP_Instance instance,
      PP_Resource share_context,
      const ppapi::Graphics3DContextAttribs& context_attribs,
      gpu::Capabilities* capabilities,
      gpu::GLCapabilities* gl_capabilities,
      const base::UnsafeSharedMemoryRegion** shared_state_region,
      gpu::CommandBufferId* command_buffer_id);

  PPB_Graphics3D_Impl(const PPB_Graphics3D_Impl&) = delete;
  PPB_Graphics3D_Impl& operator=(const PPB_Graphics3D_Impl&) = delete;

  // PPB_Graphics3D_API trusted implementation.
  PP_Bool SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                  int32_t* id) override;
  PP_Bool DestroyTransferBuffer(int32_t id) override;
  PP_Bool Flush(int32_t put_offset, uint64_t release_count) override;
  gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                int32_t end) override;
  gpu::CommandBuffer::State WaitForGetOffsetInRange(
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end) override;
  void EnsureWorkVisible() override;
  void ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                         const gpu::SyncToken& sync_token,
                         bool is_lost);
  void ResolveAndDetachFramebuffer() override;
  void DoResize(gfx::Size size) override;

  // Binds/unbinds the graphics of this context with the associated instance.
  // Returns true if binding/unbinding is successful.
  bool BindToInstance(bool bind);

  // Returns true if the backing texture is always opaque.
  bool IsOpaque();

  // Notifications about the view's progress painting.  See PluginInstance.
  // These messages are used to send Flush callbacks to the plugin.
  void ViewInitiatedPaint();

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

 protected:
  ~PPB_Graphics3D_Impl() override;
  // ppapi::PPB_Graphics3D_Shared overrides.
  gpu::CommandBuffer* GetCommandBuffer() override;
  gpu::GpuControl* GetGpuControl() override;
  int32_t DoSwapBuffers(const gpu::SyncToken& sync_token,
                        const gfx::Size& size) override;

 private:
  class ColorBuffer;

  explicit PPB_Graphics3D_Impl(PP_Instance instance);

  bool InitRaw(PPB_Graphics3D_API* share_context,
               const ppapi::Graphics3DContextAttribs& requested_attribs,
               gpu::Capabilities* capabilities,
               gpu::GLCapabilities* gl_capabilities,
               const base::UnsafeSharedMemoryRegion** shared_state_region,
               gpu::CommandBufferId* command_buffer_id);

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* msg, int id) final;
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  // Other notifications from the GPU process.
  void OnSwapBuffers();
  // Notifications sent to plugin.
  void SendContextLost();

  // This is called by NaCL process when it wants to present next frame
  // (SwapBuffers call from the plugin). Note that
  // `ResolveAndDetachFramebuffer()` must be called before and `sync_token` must
  // be submitted after that call.
  int32_t DoPresent(const gpu::SyncToken& sync_token, const gfx::Size& size);

  // Returns ColorBuffer for the next frame. It will try to re-use one of
  // `available_color_buffers_` first and create new one if there is none.
  std::unique_ptr<ColorBuffer> GetOrCreateColorBuffer();

  // This returns ColorBuffer from the display compositor. If it's not lost and
  // have the same size, it will be put in `available_color_buffers_` or
  // Destroyed otherwise.
  void RecycleColorBuffer(std::unique_ptr<ColorBuffer> buffer,
                          const gpu::SyncToken& sync_token,
                          bool is_lost);

  gfx::Size swapchain_size_;
  std::vector<std::unique_ptr<ColorBuffer>> available_color_buffers_;
  std::unique_ptr<ColorBuffer> current_color_buffer_;
  base::flat_map<gpu::Mailbox, std::unique_ptr<ColorBuffer>>
      inflight_color_buffers_;

  // True if context is bound to instance.
  bool bound_to_instance_;
  // True when waiting for compositor to commit our backing texture.
  bool commit_pending_;

#if DCHECK_IS_ON()
  bool lost_context_ = false;
#endif

  bool has_alpha_ = false;
  bool is_single_buffered_ = false;
  int samples_count_ = 0;
  bool preserve_ = false;
  bool needs_depth_ = false;
  bool needs_stencil_ = false;

  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  base::WeakPtrFactory<PPB_Graphics3D_Impl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
