// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
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
class CommandBufferProxyImpl;
struct ContextCreationAttribs;
}

namespace content {

class PPB_Graphics3D_Impl : public ppapi::PPB_Graphics3D_Shared,
                            public gpu::GpuControlClient {
 public:
  static PP_Resource CreateRaw(
      PP_Instance instance,
      PP_Resource share_context,
      const gpu::ContextCreationAttribs& attrib_helper,
      gpu::Capabilities* capabilities,
      const base::UnsafeSharedMemoryRegion** shared_state_region,
      gpu::CommandBufferId* command_buffer_id);

  // PPB_Graphics3D_API trusted implementation.
  PP_Bool SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                  int32_t* id) override;
  PP_Bool DestroyTransferBuffer(int32_t id) override;
  PP_Bool Flush(int32_t put_offset) override;
  gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                int32_t end) override;
  gpu::CommandBuffer::State WaitForGetOffsetInRange(
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end) override;
  void EnsureWorkVisible() override;
  void TakeFrontBuffer() override;
  void ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                         const gpu::SyncToken& sync_token,
                         bool is_lost);

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
  explicit PPB_Graphics3D_Impl(PP_Instance instance);

  bool InitRaw(PPB_Graphics3D_API* share_context,
               const gpu::ContextCreationAttribs& requested_attribs,
               gpu::Capabilities* capabilities,
               const base::UnsafeSharedMemoryRegion** shared_state_region,
               gpu::CommandBufferId* command_buffer_id);

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* msg, int id) final;
  void OnGpuControlSwapBuffersCompleted(
      const gpu::SwapBuffersCompleteParams& params) final;
  void OnSwapBufferPresented(uint64_t swap_id,
                             const gfx::PresentationFeedback& feedback) final {}
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  // Other notifications from the GPU process.
  void OnSwapBuffers();
  // Notifications sent to plugin.
  void SendContextLost();

  // Reuses a mailbox if one is available, otherwise makes a new one.
  gpu::Mailbox GenerateMailbox();

  // A front buffer that was recently taken from the command buffer. This should
  // be immediately consumed by DoSwapBuffers().
  gpu::Mailbox taken_front_buffer_;

  // Mailboxes that are no longer in use.
  std::vector<gpu::Mailbox> mailboxes_to_reuse_;

  // True if context is bound to instance.
  bool bound_to_instance_;
  // True when waiting for compositor to commit our backing texture.
  bool commit_pending_;

#if DCHECK_IS_ON()
  bool lost_context_ = false;
#endif

  bool has_alpha_;
  const bool use_image_chromium_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;

  base::WeakPtrFactory<PPB_Graphics3D_Impl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_GRAPHICS_3D_IMPL_H_
