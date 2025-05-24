// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_
#define PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_

#include <stdint.h>

#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace gpu {
struct Capabilities;
struct GLCapabilities;
}

namespace ppapi {

class HostResource;

namespace proxy {

class SerializedHandle;
class PpapiCommandBufferProxy;

class PPAPI_PROXY_EXPORT Graphics3D : public PPB_Graphics3D_Shared {
 public:
  Graphics3D(const HostResource& resource, const gfx::Size& size);

  Graphics3D(const Graphics3D&) = delete;
  Graphics3D& operator=(const Graphics3D&) = delete;

  ~Graphics3D() override;

  bool Init(gpu::gles2::GLES2Implementation* share_gles2,
            const gpu::Capabilities& capabilities,
            const gpu::GLCapabilities& gl_capabilities,
            SerializedHandle shared_state,
            gpu::CommandBufferId command_buffer_id);

  // Graphics3DTrusted API. These are not implemented in the proxy.
  PP_Bool SetGetBuffer(int32_t shm_id) override;
  PP_Bool Flush(int32_t put_offset, uint64_t release_count) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                  int32_t* id) override;
  PP_Bool DestroyTransferBuffer(int32_t id) override;
  gpu::CommandBuffer::State WaitForTokenInRange(int32_t start,
                                                int32_t end) override;
  gpu::CommandBuffer::State WaitForGetOffsetInRange(
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end) override;
  void EnsureWorkVisible() override;
  void ResolveAndDetachFramebuffer() override;

 private:
  // PPB_Graphics3D_Shared overrides.
  gpu::CommandBuffer* GetCommandBuffer() override;
  gpu::GpuControl* GetGpuControl() override;
  int32_t DoSwapBuffers(const gpu::SyncToken& sync_token,
                        const gfx::Size& size) override;
  void DoResize(gfx::Size size) override;

  std::unique_ptr<PpapiCommandBufferProxy> command_buffer_;
};

class PPB_Graphics3D_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Graphics3D_Proxy(Dispatcher* dispatcher);

  PPB_Graphics3D_Proxy(const PPB_Graphics3D_Proxy&) = delete;
  PPB_Graphics3D_Proxy& operator=(const PPB_Graphics3D_Proxy&) = delete;

  ~PPB_Graphics3D_Proxy();

  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_Resource share_context,
      const int32_t* attrib_list);

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  static const ApiID kApiID = API_ID_PPB_GRAPHICS_3D;

 private:
  void OnMsgCreate(PP_Instance instance,
                   HostResource share_context,
                   const Graphics3DContextAttribs& context_attribs,
                   HostResource* result,
                   gpu::Capabilities* capabilities,
                   gpu::GLCapabilities* gl_capabilities,
                   SerializedHandle* handle,
                   gpu::CommandBufferId* command_buffer_id);
  void OnMsgSetGetBuffer(const HostResource& context, int32_t id);
  void OnMsgWaitForTokenInRange(const HostResource& context,
                                int32_t start,
                                int32_t end,
                                gpu::CommandBuffer::State* state,
                                bool* success);
  void OnMsgWaitForGetOffsetInRange(const HostResource& context,
                                    uint32_t set_get_buffer_count,
                                    int32_t start,
                                    int32_t end,
                                    gpu::CommandBuffer::State* state,
                                    bool* success);
  void OnMsgAsyncFlush(const HostResource& context,
                       int32_t put_offset,
                       uint64_t release_count);
  void OnMsgCreateTransferBuffer(
      const HostResource& context,
      uint32_t size,
      int32_t* id,
      ppapi::proxy::SerializedHandle* transfer_buffer);
  void OnMsgDestroyTransferBuffer(const HostResource& context, int32_t id);
  void OnMsgSwapBuffers(const HostResource& context,
                        const gpu::SyncToken& sync_token,
                        const gfx::Size& size);
  void OnMsgEnsureWorkVisible(const HostResource& context);
  void OnMsgResolveAndDetachFramebuffer(const HostResource& context);
  void OnMsgResize(const HostResource& context, gfx::Size size);

  // Renderer->plugin message handlers.
  void OnMsgSwapBuffersACK(const HostResource& context,
                           int32_t pp_error);

  void SendSwapBuffersACKToPlugin(int32_t result,
                                  const HostResource& context);

  ProxyCompletionCallbackFactory<PPB_Graphics3D_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_
