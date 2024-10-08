// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/ppb_graphics_3d_proxy.h"

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_command_buffer_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
base::UnsafeSharedMemoryRegion TransportSHMHandle(
    Dispatcher* dispatcher,
    const base::UnsafeSharedMemoryRegion& region) {
  return dispatcher->ShareUnsafeSharedMemoryRegionWithRemote(region);
}
#endif  // !BUILDFLAG(IS_NACL)

gpu::CommandBuffer::State GetErrorState() {
  gpu::CommandBuffer::State error_state;
  error_state.error = gpu::error::kGenericError;
  return error_state;
}

}  // namespace

Graphics3D::Graphics3D(const HostResource& resource, const gfx::Size& size)
    : PPB_Graphics3D_Shared(resource, size) {}

Graphics3D::~Graphics3D() {
  DestroyGLES2Impl();
}

bool Graphics3D::Init(gpu::gles2::GLES2Implementation* share_gles2,
                      const gpu::Capabilities& capabilities,
                      const gpu::GLCapabilities& gl_capabilities,
                      SerializedHandle shared_state,
                      gpu::CommandBufferId command_buffer_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  if (!dispatcher)
    return false;

  InstanceData* data = dispatcher->GetInstanceData(host_resource().instance());
  DCHECK(data);

  command_buffer_ = std::make_unique<PpapiCommandBufferProxy>(
      host_resource(), &data->flush_info, dispatcher, capabilities,
      gl_capabilities, std::move(shared_state), command_buffer_id);

  return CreateGLES2Impl(share_gles2);
}

PP_Bool Graphics3D::SetGetBuffer(int32_t /* transfer_buffer_id */) {
  return PP_FALSE;
}

PP_Bool Graphics3D::Flush(int32_t put_offset, uint64_t release_count) {
  return PP_FALSE;
}

scoped_refptr<gpu::Buffer> Graphics3D::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  *id = -1;
  return nullptr;
}

PP_Bool Graphics3D::DestroyTransferBuffer(int32_t id) {
  return PP_FALSE;
}

gpu::CommandBuffer::State Graphics3D::WaitForTokenInRange(int32_t start,
                                                          int32_t end) {
  return GetErrorState();
}

gpu::CommandBuffer::State Graphics3D::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  return GetErrorState();
}

void Graphics3D::EnsureWorkVisible() {
  NOTREACHED();
}

void Graphics3D::ResolveAndDetachFramebuffer() {
  NOTREACHED();
}

gpu::CommandBuffer* Graphics3D::GetCommandBuffer() {
  return command_buffer_.get();
}

gpu::GpuControl* Graphics3D::GetGpuControl() {
  return command_buffer_.get();
}

int32_t Graphics3D::DoSwapBuffers(const gpu::SyncToken& sync_token,
                                  const gfx::Size& size) {
  // A valid sync token would indicate a swap buffer already happened somehow.
  DCHECK(!sync_token.HasData());

  gpu::gles2::GLES2Implementation* gl = gles2_impl();

  // Flush current GL commands.
  gl->ShallowFlushCHROMIUM();

  // Make sure we resolved and detached our frame buffer
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBGraphics3D_ResolveAndDetachFramebuffer(
          API_ID_PPB_GRAPHICS_3D, host_resource()));

  gpu::SyncToken new_sync_token;
  gl->GenSyncTokenCHROMIUM(new_sync_token.GetData());

  IPC::Message* msg = new PpapiHostMsg_PPBGraphics3D_SwapBuffers(
      API_ID_PPB_GRAPHICS_3D, host_resource(), new_sync_token, size);
  msg->set_unblock(true);
  PluginDispatcher::GetForResource(this)->Send(msg);

  return PP_OK_COMPLETIONPENDING;
}

void Graphics3D::DoResize(gfx::Size size) {
  // Flush current GL commands.
  gles2_impl()->ShallowFlushCHROMIUM();
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBGraphics3D_Resize(API_ID_PPB_GRAPHICS_3D,
                                            host_resource(), size));
}

PPB_Graphics3D_Proxy::PPB_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_Graphics3D_Proxy::~PPB_Graphics3D_Proxy() {
}

// static
PP_Resource PPB_Graphics3D_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource share_host;
  gpu::gles2::GLES2Implementation* share_gles2 = nullptr;
  if (share_context != 0) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return PP_ERROR_BADARGUMENT;

    PPB_Graphics3D_Shared* share_graphics =
        static_cast<PPB_Graphics3D_Shared*>(enter.object());
    share_host = share_graphics->host_resource();
    share_gles2 = share_graphics->gles2_impl();
  }

  Graphics3DContextAttribs attrib_helper;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr[0] != PP_GRAPHICS3DATTRIB_NONE;
         attr += 2) {
      int32_t key = attr[0];
      int32_t value = attr[1];
      switch (key) {
        case PP_GRAPHICS3DATTRIB_ALPHA_SIZE:
          attrib_helper.alpha_size = value;
          break;
        case PP_GRAPHICS3DATTRIB_DEPTH_SIZE:
          attrib_helper.depth_size = value;
          break;
        case PP_GRAPHICS3DATTRIB_STENCIL_SIZE:
          attrib_helper.stencil_size = value;
          break;
        case PP_GRAPHICS3DATTRIB_SAMPLES:
          attrib_helper.samples = value;
          break;
        case PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS:
          attrib_helper.sample_buffers = value;
          break;
        case PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR:
          attrib_helper.buffer_preserved =
              value == PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED;
          break;
        case PP_GRAPHICS3DATTRIB_WIDTH:
          attrib_helper.offscreen_framebuffer_size.set_width(value);
          break;
        case PP_GRAPHICS3DATTRIB_HEIGHT:
          attrib_helper.offscreen_framebuffer_size.set_height(value);
          break;
        case PP_GRAPHICS3DATTRIB_SINGLE_BUFFER:
          attrib_helper.single_buffer = !!value;
          break;
        // These attributes are valid, but ignored.
        case PP_GRAPHICS3DATTRIB_RED_SIZE:
        case PP_GRAPHICS3DATTRIB_BLUE_SIZE:
        case PP_GRAPHICS3DATTRIB_GREEN_SIZE:
        case PP_GRAPHICS3DATTRIB_GPU_PREFERENCE:
          break;
        default:
          DLOG(ERROR) << "Invalid context creation attribute: " << attr[0];
          return 0;
      }
    }
  }

  HostResource result;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  ppapi::proxy::SerializedHandle shared_state;
  gpu::CommandBufferId command_buffer_id;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics3D_Create(
      API_ID_PPB_GRAPHICS_3D, instance, share_host, attrib_helper, &result,
      &capabilities, &gl_capabilities, &shared_state, &command_buffer_id));

  if (result.is_null())
    return 0;

  scoped_refptr<Graphics3D> graphics_3d(
      new Graphics3D(result, attrib_helper.offscreen_framebuffer_size));
  if (!graphics_3d->Init(share_gles2, capabilities, gl_capabilities,
                         std::move(shared_state), command_buffer_id)) {
    return 0;
  }
  return graphics_3d->GetReference();
}

bool PPB_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics3D_Proxy, msg)
#if !BUILDFLAG(IS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SetGetBuffer,
                        OnMsgSetGetBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange,
                        OnMsgWaitForTokenInRange)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange,
                        OnMsgWaitForGetOffsetInRange)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_AsyncFlush, OnMsgAsyncFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                        OnMsgCreateTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                        OnMsgDestroyTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                        OnMsgSwapBuffers)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_ResolveAndDetachFramebuffer,
                        OnMsgResolveAndDetachFramebuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Resize, OnMsgResize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_EnsureWorkVisible,
                        OnMsgEnsureWorkVisible)
#endif  // !BUILDFLAG(IS_NACL)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                        OnMsgSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)

  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

#if !BUILDFLAG(IS_NACL)
void PPB_Graphics3D_Proxy::OnMsgCreate(
    PP_Instance instance,
    HostResource share_context,
    const Graphics3DContextAttribs& context_attribs,
    HostResource* result,
    gpu::Capabilities* capabilities,
    gpu::GLCapabilities* gl_capabilities,
    SerializedHandle* shared_state,
    gpu::CommandBufferId* command_buffer_id) {
  shared_state->set_null_shmem_region();

  thunk::EnterResourceCreation enter(instance);

  if (!enter.succeeded())
    return;

  const base::UnsafeSharedMemoryRegion* region = nullptr;
  result->SetHostResource(
      instance, enter.functions()->CreateGraphics3DRaw(
                    instance, share_context.host_resource(), context_attribs,
                    capabilities, gl_capabilities, &region, command_buffer_id));
  if (!result->is_null()) {
    shared_state->set_shmem_region(
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            TransportSHMHandle(dispatcher(), *region)));
  }
}

void PPB_Graphics3D_Proxy::OnMsgSetGetBuffer(const HostResource& context,
                                             int32_t transfer_buffer_id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->SetGetBuffer(transfer_buffer_id);
}

void PPB_Graphics3D_Proxy::OnMsgWaitForTokenInRange(
    const HostResource& context,
    int32_t start,
    int32_t end,
    gpu::CommandBuffer::State* state,
    bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  *state = enter.object()->WaitForTokenInRange(start, end);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgWaitForGetOffsetInRange(
    const HostResource& context,
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end,
    gpu::CommandBuffer::State* state,
    bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  *state =
      enter.object()->WaitForGetOffsetInRange(set_get_buffer_count, start, end);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                           int32_t put_offset,
                                           uint64_t release_count) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->Flush(put_offset, release_count);
}

void PPB_Graphics3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    uint32_t size,
    int32_t* id,
    SerializedHandle* transfer_buffer) {
  transfer_buffer->set_null_shmem_region();
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded()) {
    scoped_refptr<gpu::Buffer> buffer =
        enter.object()->CreateTransferBuffer(size, id);
    if (!buffer.get())
      return;
    gpu::SharedMemoryBufferBacking* backing =
        static_cast<gpu::SharedMemoryBufferBacking*>(buffer->backing());
    DCHECK(backing && backing->shared_memory_region().IsValid());
    transfer_buffer->set_shmem_region(
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            TransportSHMHandle(dispatcher(), backing->shared_memory_region())));
  } else {
    *id = -1;
  }
}

void PPB_Graphics3D_Proxy::OnMsgDestroyTransferBuffer(
    const HostResource& context,
    int32_t id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->DestroyTransferBuffer(id);
}

void PPB_Graphics3D_Proxy::OnMsgSwapBuffers(const HostResource& context,
                                            const gpu::SyncToken& sync_token,
                                            const gfx::Size& size) {
  EnterHostFromHostResourceForceCallback<PPB_Graphics3D_API> enter(
      context, callback_factory_,
      &PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin, context);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SwapBuffersWithSyncToken(
        enter.callback(), sync_token, size));
}

void PPB_Graphics3D_Proxy::OnMsgResolveAndDetachFramebuffer(
    const HostResource& context) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->ResolveAndDetachFramebuffer();
}

void PPB_Graphics3D_Proxy::OnMsgResize(const HostResource& context,
                                       gfx::Size size) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->ResizeBuffers(size.width(), size.height());
}

void PPB_Graphics3D_Proxy::OnMsgEnsureWorkVisible(const HostResource& context) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->EnsureWorkVisible();
}

#endif  // !BUILDFLAG(IS_NACL)

void PPB_Graphics3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Graphics3D_API> enter(resource);
  if (enter.succeeded())
    static_cast<Graphics3D*>(enter.object())->SwapBuffersACK(pp_error);
}

#if !BUILDFLAG(IS_NACL)
void PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& context) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics3D_SwapBuffersACK(
      API_ID_PPB_GRAPHICS_3D, context, result));
}
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace proxy
}  // namespace ppapi
