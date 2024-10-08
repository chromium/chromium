// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/ppb_graphics_3d_impl.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/thunk/enter.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/switches.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using blink::WebConsoleMessage;
using blink::WebLocalFrame;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

// This class encapsulates ColorBuffer for the plugin. It wraps corresponding
// SharedImage that we draw to and that we send to display compositor.
// Can be in one of the 3 states:
// Detached -- ColorBuffer is initialized and ready to use.
// Attached -- ColorBuffer is currently attached to the default frame buffer and
// we're drawing to it. Should be at most one in this state.
// InCompositor -- SharedImage from the ColorBuffer was sent to display
// compositor. It's considered busy until display compositor will return the
// resources.

// ColorBuffers created detached and transitioned to other states in a Detached
// => Attached => Detached => InCompositor => Detached sequence.
class PPB_Graphics3D_Impl::ColorBuffer {
 public:
  ColorBuffer(gpu::SharedImageInterface* sii,
              gfx::Size size,
              bool has_alpha,
              bool is_single_buffered)
      : sii_(sii), size_(size), is_single_buffered_(is_single_buffered) {
    gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

    if (is_single_buffered_)
      usage |= gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

    // It's possible to create Graphics3D with zero size. To avoid creating
    // shared image with zero size which will fail, we create 1x1. This matches
    // legacy behaviour where command decoders would use 1x1 for any empty
    // `offscreen_framebuffer_size`. Note, that to avoid any size mismatches, we
    // keep `size_` intact.
    auto shared_image_size = size.IsEmpty() ? gfx::Size(1, 1) : size;

    // Note, that we intentionally don't handle SCANOUT here. While
    // kPepper3DImageChromium is enabled on some CrOS devices, SkiaRenderer
    // don't support overlays for legacy mailboxes. To avoid any problems with
    // overlays, we don't introduce them here.
    client_shared_image_ = sii_->CreateSharedImage(
        {has_alpha ? viz::SinglePlaneFormat::kRGBA_8888
                   : viz::SinglePlaneFormat::kRGBX_8888,
         shared_image_size, gfx::ColorSpace::CreateSRGB(),
         kTopLeft_GrSurfaceOrigin, kUnpremul_SkAlphaType, usage,
         "PPBGraphics3DImpl"},
        gpu::SurfaceHandle());
    CHECK(client_shared_image_);

    sync_token_ = sii_->GenVerifiedSyncToken();
  }

  ~ColorBuffer() {
    DCHECK_NE(state, State::kAttached);
    sii_->DestroySharedImage(destruction_sync_token_,
                             std::move(client_shared_image_));
  }

  void Attach(gpu::CommandBufferProxyImpl* command_buffer,
              bool samples_count,
              bool preserve,
              bool needs_depth,
              bool needs_stencil) {
    DCHECK_EQ(state, State::kDetached);
    command_buffer->SetDefaultFramebufferSharedImage(
        client_shared_image_->mailbox(), sync_token_, samples_count, preserve,
        needs_depth, needs_stencil);
    state = State::kAttached;
    sync_token_.Clear();
  }

  void Detach(gpu::CommandBufferProxyImpl* command_buffer) {
    DCHECK_EQ(state, State::kAttached);
    command_buffer->SetDefaultFramebufferSharedImage(
        gpu::Mailbox(), gpu::SyncToken(), 0, false, false, false);
    state = State::kDetached;
  }

  // Note that the pointer returned from Export() is never null.
  const scoped_refptr<gpu::ClientSharedImage>& Export() {
    DCHECK_EQ(state, State::kDetached);

    // In single buffered mode we use same image regardless if it's in
    // compositor or not, so don't track here.
    if (!is_single_buffered_)
      state = State::kInCompositor;
    return client_shared_image_;
  }

  void UpdateDestructionSyncToken(const gpu::SyncToken& token) {
    destruction_sync_token_ = token;
  }

  void Recycle(const gpu::SyncToken& sync_token) {
    DCHECK_EQ(state, State::kInCompositor);
    state = State::kDetached;
    // Update both `sync_token_` which we supposed to wait on before reattaching
    // this color buffer and `destruction_sync_token_` which wait on before
    // destroying the underlying shared image, so we don't destroy it while
    // display compositor still uses it.
    sync_token_ = sync_token;
    destruction_sync_token_ = sync_token;
  }

  const gfx::Size& size() { return size_; }

  bool IsAttached() { return state == State::kAttached; }

 private:
  enum class State { kDetached, kAttached, kInCompositor };

  State state = State::kDetached;
  const raw_ptr<gpu::SharedImageInterface> sii_;
  const gfx::Size size_;
  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
  // SyncToken to wait on before re-using this color buffer.
  gpu::SyncToken sync_token_;
  // SyncToken to wait before destroying the underlying shared image.
  gpu::SyncToken destruction_sync_token_;
  const bool is_single_buffered_;
};

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PP_Instance instance)
    : PPB_Graphics3D_Shared(instance),
      bound_to_instance_(false),
      commit_pending_(false) {}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  if (current_color_buffer_ && current_color_buffer_->IsAttached()) {
    current_color_buffer_->Detach(command_buffer_.get());
  }

  current_color_buffer_.reset();
  available_color_buffers_.clear();
  inflight_color_buffers_.clear();

  // Unset the client before the command_buffer_ is destroyed, similar to how
  // WeakPtrFactory invalidates before it.
  if (command_buffer_)
    command_buffer_->SetGpuControlClient(nullptr);
}

// static
PP_Resource PPB_Graphics3D_Impl::CreateRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const ppapi::Graphics3DContextAttribs& context_attribs,
    gpu::Capabilities* capabilities,
    gpu::GLCapabilities* gl_capabilities,
    const base::UnsafeSharedMemoryRegion** shared_state_region,
    gpu::CommandBufferId* command_buffer_id) {
  PPB_Graphics3D_API* share_api = nullptr;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_api, context_attribs, capabilities,
                            gl_capabilities, shared_state_region,
                            command_buffer_id)) {
    return 0;
  }
  return graphics_3d->GetReference();
}

PP_Bool PPB_Graphics3D_Impl::SetGetBuffer(int32_t transfer_buffer_id) {
  GetCommandBuffer()->SetGetBuffer(transfer_buffer_id);
  return PP_TRUE;
}

scoped_refptr<gpu::Buffer> PPB_Graphics3D_Impl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  return GetCommandBuffer()->CreateTransferBuffer(size, id);
}

PP_Bool PPB_Graphics3D_Impl::DestroyTransferBuffer(int32_t id) {
  GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Graphics3D_Impl::Flush(int32_t put_offset, uint64_t release_count) {
  command_buffer_->UpdateLastFenceSyncRelease(release_count);
  GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  return GetCommandBuffer()->WaitForTokenInRange(start, end);
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  return GetCommandBuffer()->WaitForGetOffsetInRange(set_get_buffer_count,
                                                     start, end);
}

void PPB_Graphics3D_Impl::EnsureWorkVisible() {
  command_buffer_->EnsureWorkVisible();
}

void PPB_Graphics3D_Impl::ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                                            const gpu::SyncToken& sync_token,
                                            bool is_lost) {
  if (is_single_buffered_) {
    // We don't verify that mailbox is the same we have in the
    // `current_color_buffer_` because it could have changed do to resize.
  } else {
    auto it = inflight_color_buffers_.find(mailbox);
    CHECK(it != inflight_color_buffers_.end(), base::NotFatalUntil::M130);
    RecycleColorBuffer(std::move(it->second), sync_token, is_lost);
    inflight_color_buffers_.erase(it);
  }
}

bool PPB_Graphics3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

bool PPB_Graphics3D_Impl::IsOpaque() { return !has_alpha_; }

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

gpu::CommandBufferProxyImpl* PPB_Graphics3D_Impl::GetCommandBufferProxy() {
  DCHECK(command_buffer_);
  return command_buffer_.get();
}

gpu::CommandBuffer* PPB_Graphics3D_Impl::GetCommandBuffer() {
  return command_buffer_.get();
}

gpu::GpuControl* PPB_Graphics3D_Impl::GetGpuControl() {
  return command_buffer_.get();
}

bool PPB_Graphics3D_Impl::InitRaw(
    PPB_Graphics3D_API* share_context,
    const ppapi::Graphics3DContextAttribs& requested_attribs,
    gpu::Capabilities* capabilities,
    gpu::GLCapabilities* gl_capabilities,
    const base::UnsafeSharedMemoryRegion** shared_state_region,
    gpu::CommandBufferId* command_buffer_id) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return false;

  RenderFrame* render_frame = plugin_instance->GetRenderFrame();
  if (!render_frame)
    return false;

  const blink::web_pref::WebPreferences& prefs =
      render_frame->GetBlinkPreferences();

  // 3D access might be disabled.
  if (!prefs.pepper_3d_enabled)
    return false;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return false;
  if (render_thread->IsGpuCompositingDisabled())
    return false;

  scoped_refptr<gpu::GpuChannelHost> channel =
      render_thread->EstablishGpuChannelSync();
  if (!channel)
    return false;
  // 3D access might be blocklisted.
  if (channel->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL] ==
      gpu::kGpuFeatureStatusBlocklisted) {
    return false;
  }

  has_alpha_ = requested_attribs.alpha_size > 0;

  is_single_buffered_ = requested_attribs.single_buffer;
  needs_depth_ = requested_attribs.depth_size > 0;
  needs_stencil_ = requested_attribs.stencil_size > 0;
  swapchain_size_ = requested_attribs.offscreen_framebuffer_size;

  // If we're in single buffered mode, we don't need additional buffer to
  // preserve contents.
  preserve_ = requested_attribs.buffer_preserved && !is_single_buffered_;

  if (requested_attribs.samples > 0 && requested_attribs.sample_buffers > 0 &&
      !requested_attribs.single_buffer)
    samples_count_ = requested_attribs.samples;

  gpu::ContextCreationAttribs attrib_helper;
  attrib_helper.context_type = gpu::CONTEXT_TYPE_OPENGLES2;

  gpu::CommandBufferProxyImpl* share_buffer = nullptr;
  if (share_context) {
    PPB_Graphics3D_Impl* share_graphics =
        static_cast<PPB_Graphics3D_Impl*>(share_context);
    share_buffer = share_graphics->GetCommandBufferProxy();
  }

  shared_image_interface_ = channel->CreateClientSharedImageInterface();

  command_buffer_ = std::make_unique<gpu::CommandBufferProxyImpl>(
      std::move(channel), kGpuStreamIdDefault,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  auto result = command_buffer_->Initialize(
      gpu::kNullSurfaceHandle, share_buffer, kGpuStreamPriorityDefault,
      attrib_helper, GURL());
  if (result != gpu::ContextResult::kSuccess)
    return false;

  command_buffer_->SetGpuControlClient(this);

  if (shared_state_region)
    *shared_state_region = &command_buffer_->GetSharedStateRegion();
  if (capabilities) {
    *capabilities = command_buffer_->GetCapabilities();
  }
  if (gl_capabilities) {
    *gl_capabilities = command_buffer_->GetGLCapabilities();
  }
  if (command_buffer_id)
    *command_buffer_id = command_buffer_->GetCommandBufferID();

  current_color_buffer_ = GetOrCreateColorBuffer();
  current_color_buffer_->Attach(command_buffer_.get(), samples_count_,
                                preserve_, needs_depth_, needs_stencil_);

  return true;
}

void PPB_Graphics3D_Impl::OnGpuControlErrorMessage(const char* message,
                                                   int32_t id) {
  if (!bound_to_instance_)
    return;
  WebPluginContainer* container =
      HostGlobals::Get()->GetInstance(pp_instance())->container();
  if (!container)
    return;
  WebLocalFrame* frame = container->GetDocument().GetFrame();
  if (!frame)
    return;
  WebConsoleMessage console_message = WebConsoleMessage(
      blink::mojom::ConsoleMessageLevel::kError, WebString::FromUTF8(message));
  frame->AddMessageToConsole(console_message);
}

void PPB_Graphics3D_Impl::OnGpuControlLostContext() {
#if DCHECK_IS_ON()
  // This should never occur more than once.
  DCHECK(!lost_context_);
  lost_context_ = true;
#endif

  // Don't need to check for null from GetPluginInstance since when we're
  // bound, we know our instance is valid.
  if (bound_to_instance_) {
    HostGlobals::Get()->GetInstance(pp_instance())->BindGraphics(pp_instance(),
                                                                 0);
  }

  // Send context lost to plugin. This may have been caused by a PPAPI call, so
  // avoid re-entering.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PPB_Graphics3D_Impl::SendContextLost,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Graphics3D_Impl::OnGpuControlLostContextMaybeReentrant() {
  // No internal state to update on lost context.
}

void PPB_Graphics3D_Impl::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  NOTIMPLEMENTED();
}

void PPB_Graphics3D_Impl::OnSwapBuffers() {
  if (HasPendingSwap()) {
    // If we're off-screen, no need to trigger and wait for compositing.
    // Just send the swap-buffers ACK to the plugin immediately.
    commit_pending_ = false;
    SwapBuffersACK(PP_OK);
  }
}

void PPB_Graphics3D_Impl::SendContextLost() {
  // By the time we run this, the instance may have been deleted, or in the
  // process of being deleted. Even in the latter case, we don't want to send a
  // callback after DidDestroy.
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!instance || !instance->container())
    return;

  // This PPB_Graphics3D_Impl could be deleted during the call to
  // GetPluginInterface (which sends a sync message in some cases). We still
  // send the Graphics3DContextLost to the plugin; the instance may care about
  // that event even though this context has been destroyed.
  PP_Instance this_pp_instance = pp_instance();
  const PPP_Graphics3D* ppp_graphics_3d = static_cast<const PPP_Graphics3D*>(
      instance->module()->GetPluginInterface(PPP_GRAPHICS_3D_INTERFACE));
  // We have to check *again* that the instance exists, because it could have
  // been deleted during GetPluginInterface(). Even the PluginModule could be
  // deleted, but in that case, the instance should also be gone, so the
  // GetInstance check covers both cases.
  if (ppp_graphics_3d && HostGlobals::Get()->GetInstance(this_pp_instance))
    ppp_graphics_3d->Graphics3DContextLost(this_pp_instance);
}

int32_t PPB_Graphics3D_Impl::DoSwapBuffers(const gpu::SyncToken& sync_token,
                                           const gfx::Size& size) {
  DCHECK(command_buffer_);
  DCHECK(current_color_buffer_);
  DCHECK_EQ(size, current_color_buffer_->size());

  if (current_color_buffer_->IsAttached()) {
    DLOG(ERROR)
        << "ResolveAndDetachFramebuffer should be called before DoSwapBuffers";
    return PP_ERROR_FAILED;
  }

  current_color_buffer_->UpdateDestructionSyncToken(sync_token);

  if (bound_to_instance_) {
    // If we are bound to the instance, we need to ask the compositor
    // to commit our backing texture so that the graphics appears on the page.
    // When the backing texture will be committed we get notified via
    // ViewFlushedPaint().
    //
    // Don't need to check for NULL from GetPluginInstance since when we're
    // bound, we know our instance is valid.

    // Note, that we intentionally don't handle SCANOUT here. While
    // kPepper3DImageChromium is enabled on some CrOS devices, SkiaRenderer
    // don't support overlays for legacy mailboxes. To avoid any problems with
    // overlays, we don't introduce them here.
    constexpr bool is_overlay_candidate = false;
    constexpr uint32_t target = GL_TEXTURE_2D;
    const auto& shared_image = current_color_buffer_->Export();
    viz::TransferableResource resource = viz::TransferableResource::MakeGpu(
        shared_image, target, sync_token, current_color_buffer_->size(),
        viz::SinglePlaneFormat::kRGBA_8888, is_overlay_candidate,
        viz::TransferableResource::ResourceSource::kPPBGraphics3D);
    HostGlobals::Get()
        ->GetInstance(pp_instance())
        ->CommitTransferableResource(resource);
    commit_pending_ = true;

    if (!is_single_buffered_) {
      inflight_color_buffers_.emplace(shared_image->mailbox(),
                                      std::move(current_color_buffer_));
      current_color_buffer_ = GetOrCreateColorBuffer();
    }
  } else {
    // Wait for the command to complete on the GPU to allow for throttling.
    command_buffer_->SignalSyncToken(
        sync_token, base::BindOnce(&PPB_Graphics3D_Impl::OnSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  current_color_buffer_->Attach(command_buffer_.get(), samples_count_,
                                preserve_, needs_depth_, needs_stencil_);

  return PP_OK_COMPLETIONPENDING;
}

void PPB_Graphics3D_Impl::ResolveAndDetachFramebuffer() {
  DCHECK(current_color_buffer_);
  current_color_buffer_->Detach(command_buffer_.get());
}

void PPB_Graphics3D_Impl::DoResize(gfx::Size size) {
  if (swapchain_size_ == size)
    return;
  swapchain_size_ = size;

  // Drop all available buffers as they are wrong size now;
  available_color_buffers_.clear();

  DCHECK(current_color_buffer_);
  current_color_buffer_->Detach(command_buffer_.get());
  current_color_buffer_ = GetOrCreateColorBuffer();
  current_color_buffer_->Attach(command_buffer_.get(), samples_count_,
                                preserve_, needs_depth_, needs_stencil_);
}

std::unique_ptr<PPB_Graphics3D_Impl::ColorBuffer>
PPB_Graphics3D_Impl::GetOrCreateColorBuffer() {
  if (!available_color_buffers_.empty()) {
    auto result = std::move(*available_color_buffers_.begin());
    available_color_buffers_.erase(available_color_buffers_.begin());
    return result;
  }

  return std::make_unique<ColorBuffer>(shared_image_interface_.get(),
                                       swapchain_size_, has_alpha_,
                                       is_single_buffered_);
}

void PPB_Graphics3D_Impl::RecycleColorBuffer(
    std::unique_ptr<ColorBuffer> buffer,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  buffer->Recycle(sync_token);
  if (is_lost || buffer->size() != swapchain_size_)
    return;

  available_color_buffers_.push_back(std::move(buffer));
}

}  // namespace content
