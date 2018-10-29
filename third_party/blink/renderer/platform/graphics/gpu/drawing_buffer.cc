/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_contents.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

const float kResourceAdjustedRatio = 0.5;

static bool g_should_fail_drawing_buffer_creation_for_testing = false;

}  // namespace

scoped_refptr<DrawingBuffer> DrawingBuffer::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    Client* client,
    const IntSize& size,
    bool premultiplied_alpha,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool want_antialiasing,
    PreserveDrawingBuffer preserve,
    WebGLVersion webgl_version,
    ChromiumImageUsage chromium_image_usage,
    const CanvasColorParams& color_params) {
  if (g_should_fail_drawing_buffer_creation_for_testing) {
    g_should_fail_drawing_buffer_creation_for_testing = false;
    return nullptr;
  }

  base::CheckedNumeric<int> data_size = color_params.BytesPerPixel();
  data_size *= size.Width();
  data_size *= size.Height();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return nullptr;

  DCHECK(context_provider);
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(context_provider->ContextGL());
  if (!extensions_util->IsValid()) {
    // This might be the first time we notice that the GL context is lost.
    return nullptr;
  }
  DCHECK(extensions_util->SupportsExtension("GL_OES_packed_depth_stencil"));
  extensions_util->EnsureExtensionEnabled("GL_OES_packed_depth_stencil");
  bool multisample_supported =
      want_antialiasing &&
      (extensions_util->SupportsExtension(
           "GL_CHROMIUM_framebuffer_multisample") ||
       extensions_util->SupportsExtension(
           "GL_EXT_multisampled_render_to_texture")) &&
      extensions_util->SupportsExtension("GL_OES_rgb8_rgba8");
  if (multisample_supported) {
    extensions_util->EnsureExtensionEnabled("GL_OES_rgb8_rgba8");
    if (extensions_util->SupportsExtension(
            "GL_CHROMIUM_framebuffer_multisample")) {
      extensions_util->EnsureExtensionEnabled(
          "GL_CHROMIUM_framebuffer_multisample");
    } else {
      extensions_util->EnsureExtensionEnabled(
          "GL_EXT_multisampled_render_to_texture");
    }
  }
  bool discard_framebuffer_supported =
      extensions_util->SupportsExtension("GL_EXT_discard_framebuffer");
  if (discard_framebuffer_supported)
    extensions_util->EnsureExtensionEnabled("GL_EXT_discard_framebuffer");

  scoped_refptr<DrawingBuffer> drawing_buffer =
      base::AdoptRef(new DrawingBuffer(
          std::move(context_provider), using_gpu_compositing,
          std::move(extensions_util), client, discard_framebuffer_supported,
          want_alpha_channel, premultiplied_alpha, preserve, webgl_version,
          want_depth_buffer, want_stencil_buffer, chromium_image_usage,
          color_params));
  if (!drawing_buffer->Initialize(size, multisample_supported)) {
    drawing_buffer->BeginDestruction();
    return scoped_refptr<DrawingBuffer>();
  }
  return drawing_buffer;
}

void DrawingBuffer::ForceNextDrawingBufferCreationToFail() {
  g_should_fail_drawing_buffer_creation_for_testing = true;
}

DrawingBuffer::DrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    std::unique_ptr<Extensions3DUtil> extensions_util,
    Client* client,
    bool discard_framebuffer_supported,
    bool want_alpha_channel,
    bool premultiplied_alpha,
    PreserveDrawingBuffer preserve,
    WebGLVersion webgl_version,
    bool want_depth,
    bool want_stencil,
    ChromiumImageUsage chromium_image_usage,
    const CanvasColorParams& color_params)
    : client_(client),
      preserve_drawing_buffer_(preserve),
      webgl_version_(webgl_version),
      context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      gl_(this->ContextProvider()->ContextGL()),
      extensions_util_(std::move(extensions_util)),
      discard_framebuffer_supported_(discard_framebuffer_supported),
      want_alpha_channel_(want_alpha_channel),
      premultiplied_alpha_(premultiplied_alpha),
      using_gpu_compositing_(using_gpu_compositing),
      want_depth_(want_depth),
      want_stencil_(want_stencil),
      storage_color_space_(color_params.GetStorageGfxColorSpace()),
      sampler_color_space_(color_params.GetSamplerGfxColorSpace()),
      use_half_float_storage_(color_params.PixelFormat() ==
                              kF16CanvasPixelFormat),
      chromium_image_usage_(chromium_image_usage),
      opengl_flip_y_extension_(
          ContextProvider()->GetCapabilities().mesa_framebuffer_flip_y) {
  // Used by browser tests to detect the use of a DrawingBuffer.
  TRACE_EVENT_INSTANT0("test_gpu", "DrawingBufferCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

DrawingBuffer::~DrawingBuffer() {
  DCHECK(destruction_in_progress_);
  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }
  context_provider_ = nullptr;
}

bool DrawingBuffer::MarkContentsChanged() {
  if (contents_change_resolved_ || !contents_changed_) {
    contents_change_resolved_ = false;
    contents_changed_ = true;
    return true;
  }
  return false;
}

void DrawingBuffer::ResetBuffersToAutoClear() {
  GLuint buffers = GL_COLOR_BUFFER_BIT;
  if (want_depth_)
    buffers |= GL_DEPTH_BUFFER_BIT;
  if (want_stencil_ || has_implicit_stencil_buffer_)
    buffers |= GL_STENCIL_BUFFER_BIT;
  SetBuffersToAutoClear(buffers);
}

void DrawingBuffer::SetBuffersToAutoClear(GLbitfield buffers) {
  if (preserve_drawing_buffer_ == kDiscard) {
    buffers_to_auto_clear_ = buffers;
  } else {
    DCHECK_EQ(0u, buffers_to_auto_clear_);
  }
}

GLbitfield DrawingBuffer::GetBuffersToAutoClear() const {
  return buffers_to_auto_clear_;
}

gpu::gles2::GLES2Interface* DrawingBuffer::ContextGL() {
  return gl_;
}

WebGraphicsContext3DProvider* DrawingBuffer::ContextProvider() {
  return context_provider_->ContextProvider();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DrawingBuffer::ContextProviderWeakPtr() {
  return context_provider_->GetWeakPtr();
}

const DrawingBuffer::WebGLContextLimits& DrawingBuffer::webgl_context_limits() {
  return webgl_context_limits_;
}

void DrawingBuffer::SetIsHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;
  is_hidden_ = hidden;
  if (is_hidden_)
    recycled_color_buffer_queue_.clear();
}

void DrawingBuffer::SetFilterQuality(SkFilterQuality filter_quality) {
  if (filter_quality_ != filter_quality) {
    filter_quality_ = filter_quality;
    if (layer_)
      layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
  }
}

bool DrawingBuffer::RequiresAlphaChannelToBePreserved() {
  return client_->DrawingBufferClientIsBoundForDraw() &&
         DefaultBufferRequiresAlphaChannelToBePreserved();
}

bool DrawingBuffer::DefaultBufferRequiresAlphaChannelToBePreserved() {
  return !want_alpha_channel_ && have_alpha_channel_;
}

DrawingBuffer::RegisteredBitmap DrawingBuffer::CreateOrRecycleBitmap(
    cc::SharedBitmapIdRegistrar* bitmap_registrar) {
  // When searching for a hit in SharedBitmap, we don't consider the bitmap
  // format (RGBA 8888 vs F16). We expect to always have the same bitmap format,
  // matching the back storage of the drawing buffer.
  auto* it = std::remove_if(recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
                            [this](const RegisteredBitmap& registered) {
                              return registered.bitmap->size() !=
                                     static_cast<gfx::Size>(size_);
                            });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RegisteredBitmap recycled = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(recycled.bitmap->size() == static_cast<gfx::Size>(size_));
    return recycled;
  }

  viz::SharedBitmapId id = viz::SharedBitmap::GenerateId();
  viz::ResourceFormat format = viz::RGBA_8888;
  if (use_half_float_storage_)
    format = viz::RGBA_F16;
  std::unique_ptr<base::SharedMemory> shm =
      viz::bitmap_allocation::AllocateMappedBitmap(
          static_cast<gfx::Size>(size_), format);
  auto bitmap = base::MakeRefCounted<cc::CrossThreadSharedBitmap>(
      id, std::move(shm), static_cast<gfx::Size>(size_), format);
  RegisteredBitmap registered = {
      bitmap, bitmap_registrar->RegisterSharedBitmapId(id, bitmap)};
  return registered;
}

bool DrawingBuffer::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  ScopedStateRestorer scoped_state_restorer(this);
  bool force_gpu_result = false;
  return PrepareTransferableResourceInternal(
      bitmap_registrar, out_resource, out_release_callback, force_gpu_result);
}

bool DrawingBuffer::PrepareTransferableResourceInternal(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback,
    bool force_gpu_result) {
  DCHECK(state_restorer_);
  if (destruction_in_progress_) {
    // It can be hit in the following sequence.
    // 1. WebGL draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context lost using WEBGL_lose_context extension.
    // 4. Here.
    return false;
  }
  DCHECK(!is_hidden_);
  if (!contents_changed_)
    return false;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return false;

  TRACE_EVENT0("blink,rail", "DrawingBuffer::prepareMailbox");

  // Resolve the multisampled buffer into the texture attached to fbo_.
  ResolveIfNeeded();

  if (!using_gpu_compositing_ && !force_gpu_result) {
    FinishPrepareTransferableResourceSoftware(bitmap_registrar, out_resource,
                                              out_release_callback);
  } else {
    FinishPrepareTransferableResourceGpu(out_resource, out_release_callback);
  }
  return true;
}

void DrawingBuffer::FinishPrepareTransferableResourceSoftware(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  DCHECK(state_restorer_);
  RegisteredBitmap registered = CreateOrRecycleBitmap(bitmap_registrar);

  // Read the framebuffer into |bitmap|.
  {
    unsigned char* pixels = static_cast<unsigned char*>(
        registered.bitmap->shared_memory()->memory());
    DCHECK(pixels);
    bool need_premultiply = want_alpha_channel_ && !premultiplied_alpha_;
    WebGLImageConversion::AlphaOp op =
        need_premultiply ? WebGLImageConversion::kAlphaDoPremultiply
                         : WebGLImageConversion::kAlphaDoNothing;
    state_restorer_->SetFramebufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    ReadBackFramebuffer(pixels, Size().Width(), Size().Height(), kReadbackSkia,
                        op);
  }

  viz::ResourceFormat format = viz::RGBA_8888;
  if (use_half_float_storage_)
    format = viz::RGBA_F16;
  *out_resource = viz::TransferableResource::MakeSoftware(
      registered.bitmap->id(), static_cast<gfx::Size>(size_), format);
  out_resource->color_space = storage_color_space_;

  // This holds a ref on the DrawingBuffer that will keep it alive until the
  // mailbox is released (and while the release callback is running). It also
  // owns the SharedBitmap.
  auto func = WTF::Bind(&DrawingBuffer::MailboxReleasedSoftware,
                        scoped_refptr<DrawingBuffer>(this),
                        WTF::Passed(std::move(registered)));
  *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));

  ResetBuffersToAutoClear();
}

void DrawingBuffer::FinishPrepareTransferableResourceGpu(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  DCHECK(state_restorer_);
  if (webgl_version_ > kWebGL1) {
    state_restorer_->SetPixelUnpackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  if (premultiplied_alpha_false_texture_) {
    // The rendering results are in this texture rather than the
    // back_color_buffer_'s texture. Copy them in, multiplying the alpha channel
    // into the color channels.
    gl_->CopySubTextureCHROMIUM(premultiplied_alpha_false_texture_, 0,
                                texture_target_, back_color_buffer_->texture_id,
                                0, 0, 0, 0, 0, size_.Width(), size_.Height(),
                                GL_FALSE, GL_TRUE, GL_FALSE);
  }

  // Specify the buffer that we will put in the mailbox.
  scoped_refptr<ColorBuffer> color_buffer_for_mailbox;
  if (preserve_drawing_buffer_ == kDiscard) {
    // If we can discard the backbuffer, send the old backbuffer directly
    // into the mailbox, and allocate (or recycle) a new backbuffer.
    color_buffer_for_mailbox = back_color_buffer_;
    back_color_buffer_ = CreateOrRecycleColorBuffer();
    AttachColorBufferToReadFramebuffer();

    // Explicitly specify that m_fbo (which is now bound to the just-allocated
    // m_backColorBuffer) is not initialized, to save GPU memory bandwidth for
    // tile-based GPU architectures.
    if (discard_framebuffer_supported_) {
      const GLenum kAttachments[3] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT,
                                      GL_STENCIL_ATTACHMENT};
      state_restorer_->SetFramebufferBindingDirty();
      gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
      gl_->DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, kAttachments);
    }
  } else {
    // If we can't discard the backbuffer, create (or recycle) a buffer to put
    // in the mailbox, and copy backbuffer's contents there.
    color_buffer_for_mailbox = CreateOrRecycleColorBuffer();
    gl_->CopySubTextureCHROMIUM(
        back_color_buffer_->texture_id, 0, texture_target_,
        color_buffer_for_mailbox->texture_id, 0, 0, 0, 0, 0, size_.Width(),
        size_.Height(), GL_FALSE, GL_FALSE, GL_FALSE);
  }

  // Put colorBufferForMailbox into its mailbox, and populate its
  // produceSyncToken with that point.
  {
    // It's critical to order the execution of this context's work relative
    // to other contexts, in particular the compositor. Previously this
    // used to be a Flush, and there was a bug that we didn't flush before
    // synchronizing with the composition, and on some platforms this caused
    // incorrect rendering with complex WebGL content that wasn't always
    // properly flushed to the driver. There is now a basic assumption that
    // there are implicit flushes between contexts at the lowest level.
    gl_->GenUnverifiedSyncTokenCHROMIUM(
        color_buffer_for_mailbox->produce_sync_token.GetData());
#if defined(OS_MACOSX) || defined(OS_ANDROID)
    // Needed for GPU back-pressure on macOS and Android. Used to be in the
    // middle of the commands above; try to move it to the bottom to allow them
    // to be treated atomically.
    gl_->DescheduleUntilFinishedCHROMIUM();
#endif
  }

  // Populate the output mailbox and callback.
  {
    bool is_overlay_candidate = color_buffer_for_mailbox->image_id != 0;
    *out_resource = viz::TransferableResource::MakeGLOverlay(
        color_buffer_for_mailbox->mailbox, GL_LINEAR, texture_target_,
        color_buffer_for_mailbox->produce_sync_token, gfx::Size(size_),
        is_overlay_candidate);
    out_resource->color_space = sampler_color_space_;
    out_resource->format = viz::RGBA_8888;
    if (use_half_float_storage_)
      out_resource->format = viz::RGBA_F16;

    // This holds a ref on the DrawingBuffer that will keep it alive until the
    // mailbox is released (and while the release callback is running).
    auto func =
        WTF::Bind(&DrawingBuffer::MailboxReleasedGpu,
                  scoped_refptr<DrawingBuffer>(this), color_buffer_for_mailbox);
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  }

  // Point |m_frontColorBuffer| to the buffer that we are now presenting.
  front_color_buffer_ = color_buffer_for_mailbox;

  contents_changed_ = false;
  ResetBuffersToAutoClear();
}

void DrawingBuffer::MailboxReleasedGpu(scoped_refptr<ColorBuffer> color_buffer,
                                       const gpu::SyncToken& sync_token,
                                       bool lost_resource) {
  // If the mailbox has been returned by the compositor then it is no
  // longer being presented, and so is no longer the front buffer.
  if (color_buffer == front_color_buffer_)
    front_color_buffer_ = nullptr;

  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  color_buffer->receive_sync_token = sync_token;

  if (destruction_in_progress_ || color_buffer->size != size_ ||
      gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR || lost_resource ||
      is_hidden_) {
    return;
  }

  // Creation of image backed mailboxes is very expensive, so be less
  // aggressive about pruning them. Pruning is done in FIFO order.
  size_t cache_limit = 1;
  if (ShouldUseChromiumImage())
    cache_limit = 4;
  while (recycled_color_buffer_queue_.size() >= cache_limit)
    recycled_color_buffer_queue_.TakeLast();

  recycled_color_buffer_queue_.push_front(color_buffer);
}

void DrawingBuffer::MailboxReleasedSoftware(RegisteredBitmap registered,
                                            const gpu::SyncToken& sync_token,
                                            bool lost_resource) {
  DCHECK(!sync_token.HasData());  // No sync tokens for software resources.
  if (destruction_in_progress_ || lost_resource || is_hidden_ ||
      registered.bitmap->size() != static_cast<gfx::Size>(size_)) {
    // Just delete the RegisteredBitmap, which will free the memory and
    // unregister it with the compositor.
    return;
  }

  recycled_bitmaps_.push_back(std::move(registered));
}

scoped_refptr<StaticBitmapImage> DrawingBuffer::TransferToStaticBitmapImage(
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  ScopedStateRestorer scoped_state_restorer(this);

  viz::TransferableResource transferable_resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  constexpr bool force_gpu_result = true;
  if (!PrepareTransferableResourceInternal(nullptr, &transferable_resource,
                                           &release_callback,
                                           force_gpu_result)) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost. We intentionally leave the transparent black image in legacy color
    // space.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
    return StaticBitmapImage::Create(surface->makeImageSnapshot());
  }

  DCHECK_EQ(size_.Width(), transferable_resource.size.width());
  DCHECK_EQ(size_.Height(), transferable_resource.size.height());

  // Make our own textureId that is a reference on the same texture backing
  // being used as the front buffer (which was returned from
  // PrepareTransferableResourceInternal()). We do not need to wait on the sync
  // token in |transferable_resource| since the mailbox was produced on the same
  // |m_gl| context that we are using here. Similarly, the |release_callback|
  // will run on the same context so we don't need to send a sync token for this
  // consume action back to it.
  // TODO(danakj): Instead of using PrepareTransferableResourceInternal(), we
  // could just use the actual texture id and avoid needing to produce/consume a
  // mailbox.
  GLuint texture_id = gl_->CreateAndConsumeTextureCHROMIUM(
      transferable_resource.mailbox_holder.mailbox.name);

  if (out_release_callback) {
    // Allow the consumer to release the resource when done using it, so it can
    // be recycled.
    *out_release_callback = std::move(release_callback);
  } else {
    // Return the mailbox but report that the resource is lost to prevent trying
    // to use the backing for future frames. We keep it alive with our own
    // reference to the backing via our |textureId|.
    release_callback->Run(gpu::SyncToken(), true /* lost_resource */);
  }

  // We reuse the same mailbox name from above since our texture id was consumed
  // from it.
  const auto& sk_image_mailbox = transferable_resource.mailbox_holder.mailbox;
  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid. We wouldn't need to wait for the consume done in this function
  // because the texture id it generated would only be valid for the
  // DrawingBuffer's context anyways.
  const auto& sk_image_sync_token =
      transferable_resource.mailbox_holder.sync_token;

  // TODO(xidachen): Create a small pool of recycled textures from
  // ImageBitmapRenderingContext's transferFromImageBitmap, and try to use them
  // in DrawingBuffer.
  return AcceleratedStaticBitmapImage::CreateFromWebGLContextImage(
      sk_image_mailbox, sk_image_sync_token, texture_id,
      context_provider_->GetWeakPtr(), size_);
}

scoped_refptr<DrawingBuffer::ColorBuffer> DrawingBuffer::CreateOrRecycleColorBuffer() {
  DCHECK(state_restorer_);
  if (!recycled_color_buffer_queue_.IsEmpty()) {
    scoped_refptr<ColorBuffer> recycled =
        recycled_color_buffer_queue_.TakeLast();
    if (recycled->receive_sync_token.HasData())
      gl_->WaitSyncTokenCHROMIUM(recycled->receive_sync_token.GetData());
    DCHECK(recycled->size == size_);
    return recycled;
  }
  return CreateColorBuffer(size_);
}

DrawingBuffer::ScopedRGBEmulationForBlitFramebuffer::
    ScopedRGBEmulationForBlitFramebuffer(DrawingBuffer* drawing_buffer,
                                         bool is_user_draw_framebuffer_bound)
    : drawing_buffer_(drawing_buffer) {
  doing_work_ = drawing_buffer->SetupRGBEmulationForBlitFramebuffer(
      is_user_draw_framebuffer_bound);
}

DrawingBuffer::ScopedRGBEmulationForBlitFramebuffer::
    ~ScopedRGBEmulationForBlitFramebuffer() {
  if (doing_work_) {
    drawing_buffer_->CleanupRGBEmulationForBlitFramebuffer();
  }
}

DrawingBuffer::ColorBuffer::ColorBuffer(
    DrawingBuffer* drawing_buffer,
    const IntSize& size,
    GLuint texture_id,
    GLuint image_id,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer)
    : drawing_buffer(drawing_buffer),
      size(size),
      texture_id(texture_id),
      image_id(image_id),
      gpu_memory_buffer(std::move(gpu_memory_buffer)) {
  gpu::gles2::GLES2Interface* gl = drawing_buffer->ContextGL();
  gl->ProduceTextureDirectCHROMIUM(texture_id, mailbox.name);
}

DrawingBuffer::ColorBuffer::~ColorBuffer() {
  gpu::gles2::GLES2Interface* gl = drawing_buffer->gl_;
  GLenum texture_target = drawing_buffer->texture_target_;
  if (receive_sync_token.HasData())
    gl->WaitSyncTokenCHROMIUM(receive_sync_token.GetConstData());
  if (image_id) {
    gl->BindTexture(texture_target, texture_id);
    gl->ReleaseTexImage2DCHROMIUM(texture_target, image_id);
    if (rgb_workaround_texture_id) {
      gl->BindTexture(texture_target, rgb_workaround_texture_id);
      gl->ReleaseTexImage2DCHROMIUM(texture_target, image_id);
    }
    gl->DestroyImageCHROMIUM(image_id);
    switch (texture_target) {
      case GL_TEXTURE_2D:
        // Restore the texture binding for GL_TEXTURE_2D, since the client will
        // expect the previous state.
        if (drawing_buffer->client_)
          drawing_buffer->client_->DrawingBufferClientRestoreTexture2DBinding();
        break;
      case GC3D_TEXTURE_RECTANGLE_ARB:
        // Rectangle textures aren't exposed to WebGL, so don't bother
        // restoring this state (there is no meaningful way to restore it).
        break;
      default:
        NOTREACHED();
        break;
    }
    gpu_memory_buffer.reset();
  }
  gl->DeleteTextures(1, &texture_id);
  if (rgb_workaround_texture_id) {
    // Avoid deleting this texture if it was unused.
    gl->DeleteTextures(1, &rgb_workaround_texture_id);
  }
}

bool DrawingBuffer::Initialize(const IntSize& size, bool use_multisampling) {
  ScopedStateRestorer scoped_state_restorer(this);

  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // Need to try to restore the context again later.
    DLOG(ERROR) << "Cannot initialize with lost context.";
    return false;
  }

  // Specifying a half-float backbuffer requires and implicitly enables
  // half-float backbuffer extensions.
  if (use_half_float_storage_) {
    const char* color_buffer_extension = webgl_version_ > kWebGL1
                                             ? "GL_EXT_color_buffer_float"
                                             : "GL_EXT_color_buffer_half_float";
    if (!extensions_util_->EnsureExtensionEnabled(color_buffer_extension)) {
      DLOG(ERROR) << "Half-float color buffer support is absent.";
      return false;
    }
    // Support for RGB half-float renderbuffers is absent from ES3. Do not
    // attempt to expose them.
    if (!want_alpha_channel_) {
      DLOG(ERROR) << "RGB half-float renderbuffers are not supported.";
      return false;
    }
  }

  gl_->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);

  auto webgl_preferences =
      ContextProvider()->GetGpuFeatureInfo().webgl_preferences;
  webgl_context_limits_.max_active_webgl_contexts =
      webgl_preferences.max_active_webgl_contexts;
  webgl_context_limits_.max_active_webgl_contexts_on_worker =
      webgl_preferences.max_active_webgl_contexts_on_worker;

  int max_sample_count = 0;
  if (use_multisampling) {
    gl_->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &max_sample_count);
  }
  if (webgl_preferences.anti_aliasing_mode ==
      gpu::kAntialiasingModeUnspecified) {
    if (use_multisampling) {
      anti_aliasing_mode_ = gpu::kAntialiasingModeMSAAExplicitResolve;
      if (extensions_util_->SupportsExtension(
              "GL_EXT_multisampled_render_to_texture")) {
        anti_aliasing_mode_ = gpu::kAntialiasingModeMSAAImplicitResolve;
      } else if (extensions_util_->SupportsExtension(
                     "GL_CHROMIUM_screen_space_antialiasing") &&
                 !ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
                     gpu::DISABLE_FRAMEBUFFER_CMAA)) {
        anti_aliasing_mode_ = gpu::kAntialiasingModeScreenSpaceAntialiasing;
      }
    } else {
      anti_aliasing_mode_ = gpu::kAntialiasingModeNone;
    }
  } else {
    if ((webgl_preferences.anti_aliasing_mode ==
             gpu::kAntialiasingModeMSAAImplicitResolve &&
         !extensions_util_->SupportsExtension(
             "GL_EXT_multisampled_render_to_texture")) ||
        (webgl_preferences.anti_aliasing_mode ==
             gpu::kAntialiasingModeScreenSpaceAntialiasing &&
         !extensions_util_->SupportsExtension(
             "GL_CHROMIUM_screen_space_antialiasing"))) {
      DLOG(ERROR) << "Invalid anti-aliasing mode specified.";
      return false;
    }
    anti_aliasing_mode_ = webgl_preferences.anti_aliasing_mode;
  }

  // TODO(dshwang): Enable storage textures on all platforms. crbug.com/557848
  // The Linux ATI bot fails
  // WebglConformance.conformance_textures_misc_tex_image_webgl, so use storage
  // textures only if ScreenSpaceAntialiasing is enabled, because
  // ScreenSpaceAntialiasing is much faster with storage textures.
  storage_texture_supported_ =
      (webgl_version_ > kWebGL1 ||
       extensions_util_->SupportsExtension("GL_EXT_texture_storage")) &&
      anti_aliasing_mode_ == gpu::kAntialiasingModeScreenSpaceAntialiasing;

  sample_count_ = std::min(
      static_cast<int>(webgl_preferences.msaa_sample_count), max_sample_count);

  texture_target_ = GL_TEXTURE_2D;
#if defined(OS_MACOSX)
  if (ShouldUseChromiumImage()) {
    // A CHROMIUM_image backed texture requires a specialized set of parameters
    // on OSX.
    texture_target_ = GC3D_TEXTURE_RECTANGLE_ARB;
  }
#endif

  // Initialize the alpha allocation settings based on the features and
  // workarounds in use.
  if (want_alpha_channel_) {
    allocate_alpha_channel_ = true;
    have_alpha_channel_ = true;
  } else {
    allocate_alpha_channel_ = false;
    have_alpha_channel_ = false;
    if (ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
            gpu::DISABLE_GL_RGB_FORMAT)) {
      // This configuration will
      //  - allow invalid CopyTexImage to RGBA targets
      //  - fail valid FramebufferBlit from RGB targets
      // https://crbug.com/776269
      allocate_alpha_channel_ = true;
      have_alpha_channel_ = true;
    }
    if (WantExplicitResolve() &&
        ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
            gpu::DISABLE_WEBGL_RGB_MULTISAMPLING_USAGE)) {
      // This configuration avoids the above issues because
      //  - CopyTexImage is invalid from multisample renderbuffers
      //  - FramebufferBlit is invalid to multisample renderbuffers
      allocate_alpha_channel_ = true;
      have_alpha_channel_ = true;
    }
    if (ShouldUseChromiumImage() &&
        ContextProvider()->GetCapabilities().chromium_image_rgb_emulation) {
      // This configuration avoids the above issues by
      //  - extra command buffer validation for CopyTexImage
      //  - explicity re-binding as RGB for FramebufferBlit
      allocate_alpha_channel_ = false;
      have_alpha_channel_ = true;
    }
  }

  state_restorer_->SetFramebufferBindingDirty();
  gl_->GenFramebuffers(1, &fbo_);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  if (opengl_flip_y_extension_)
    gl_->FramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);

  if (WantExplicitResolve()) {
    gl_->GenFramebuffers(1, &multisample_fbo_);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    gl_->GenRenderbuffers(1, &multisample_renderbuffer_);
    if (opengl_flip_y_extension_)
      gl_->FramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);
  }
  if (!ResizeFramebufferInternal(size)) {
    DLOG(ERROR) << "Initialization failed to allocate backbuffer.";
    return false;
  }

  if (depth_stencil_buffer_) {
    DCHECK(WantDepthOrStencil());
    has_implicit_stencil_buffer_ = !want_stencil_;
  }

  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // It's possible that the drawing buffer allocation provokes a context loss,
    // so check again just in case. http://crbug.com/512302
    DLOG(ERROR) << "Context lost during initialization.";
    return false;
  }

  return true;
}

bool DrawingBuffer::CopyToPlatformTexture(gpu::gles2::GLES2Interface* dst_gl,
                                          GLenum dst_texture_target,
                                          GLuint dst_texture,
                                          bool premultiply_alpha,
                                          bool flip_y,
                                          const IntPoint& dst_texture_offset,
                                          const IntRect& src_sub_rectangle,
                                          SourceDrawingBuffer src_buffer) {
  ScopedStateRestorer scoped_state_restorer(this);

  gpu::gles2::GLES2Interface* src_gl = gl_;

  if (contents_changed_) {
    ResolveIfNeeded();
    src_gl->Flush();
  }

  if (!Extensions3DUtil::CanUseCopyTextureCHROMIUM(dst_texture_target))
    return false;

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  gpu::Mailbox mailbox;
  gpu::SyncToken produce_sync_token;
  if (src_buffer == kFrontBuffer && front_color_buffer_) {
    mailbox = front_color_buffer_->mailbox;
    produce_sync_token = front_color_buffer_->produce_sync_token;
  } else {
    if (premultiplied_alpha_false_texture_) {
      // If this texture exists, then it holds the rendering results at this
      // point, rather than back_color_buffer_. back_color_buffer_ receives the
      // contents of this texture later, premultiplying alpha into the color
      // channels. We lazily produce a mailbox for it.
      if (premultiplied_alpha_false_mailbox_.IsZero()) {
        src_gl->ProduceTextureDirectCHROMIUM(
            premultiplied_alpha_false_texture_,
            premultiplied_alpha_false_mailbox_.name);
      }
      mailbox = premultiplied_alpha_false_mailbox_;
    } else {
      mailbox = back_color_buffer_->mailbox;
    }
    src_gl->GenUnverifiedSyncTokenCHROMIUM(produce_sync_token.GetData());
  }

  if (!produce_sync_token.HasData()) {
    // This should only happen if the context has been lost.
    return false;
  }

  dst_gl->WaitSyncTokenCHROMIUM(produce_sync_token.GetConstData());
  GLuint src_texture = dst_gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);

  GLboolean unpack_premultiply_alpha_needed = GL_FALSE;
  GLboolean unpack_unpremultiply_alpha_needed = GL_FALSE;
  if (want_alpha_channel_ && premultiplied_alpha_ && !premultiply_alpha)
    unpack_unpremultiply_alpha_needed = GL_TRUE;
  else if (want_alpha_channel_ && !premultiplied_alpha_ && premultiply_alpha)
    unpack_premultiply_alpha_needed = GL_TRUE;

  dst_gl->CopySubTextureCHROMIUM(
      src_texture, 0, dst_texture_target, dst_texture, 0,
      dst_texture_offset.X(), dst_texture_offset.Y(), src_sub_rectangle.X(),
      src_sub_rectangle.Y(), src_sub_rectangle.Width(),
      src_sub_rectangle.Height(), flip_y, unpack_premultiply_alpha_needed,
      unpack_unpremultiply_alpha_needed);

  dst_gl->DeleteTextures(1, &src_texture);

  gpu::SyncToken sync_token;
  dst_gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  src_gl->WaitSyncTokenCHROMIUM(sync_token.GetData());

  return true;
}

cc::Layer* DrawingBuffer::CcLayer() {
  if (!layer_) {
    layer_ = cc::TextureLayer::CreateForMailbox(this);

    layer_->SetIsDrawable(true);
    layer_->SetContentsOpaque(!want_alpha_channel_);
    layer_->SetBlendBackgroundColor(want_alpha_channel_);
    // If premultiplied_alpha_false_texture_ exists, then premultiplied_alpha_
    // has already been handled via CopySubTextureCHROMIUM -- the alpha channel
    // has been multiplied into the color channels. In this case, or if
    // premultiplied_alpha_ is true, then the layer should consider its contents
    // to be premultiplied.
    //
    // The only situation where the layer should consider its contents
    // un-premultiplied is when premultiplied_alpha_ is false, and
    // premultiplied_alpha_false_texture_ does not exist.
    DCHECK(!(premultiplied_alpha_ && premultiplied_alpha_false_texture_));
    layer_->SetPremultipliedAlpha(premultiplied_alpha_ ||
                                  premultiplied_alpha_false_texture_);
    layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);

    if (opengl_flip_y_extension_)
      layer_->SetFlipped(false);

    GraphicsLayer::RegisterContentsLayer(layer_.get());
  }

  return layer_.get();
}

void DrawingBuffer::ClearCcLayer() {
  if (layer_)
    layer_->ClearTexture();

  gl_->Flush();
}

void DrawingBuffer::BeginDestruction() {
  DCHECK(!destruction_in_progress_);
  destruction_in_progress_ = true;

  ClearCcLayer();
  recycled_color_buffer_queue_.clear();

  // If the drawing buffer is being destroyed due to a real context loss these
  // calls will be ineffective, but won't be harmful.
  if (multisample_fbo_)
    gl_->DeleteFramebuffers(1, &multisample_fbo_);

  if (fbo_)
    gl_->DeleteFramebuffers(1, &fbo_);

  if (multisample_renderbuffer_)
    gl_->DeleteRenderbuffers(1, &multisample_renderbuffer_);

  if (depth_stencil_buffer_)
    gl_->DeleteRenderbuffers(1, &depth_stencil_buffer_);

  if (premultiplied_alpha_false_texture_) {
    gl_->DeleteTextures(1, &premultiplied_alpha_false_texture_);
    premultiplied_alpha_false_mailbox_.SetZero();
  }

  size_ = IntSize();

  back_color_buffer_ = nullptr;
  front_color_buffer_ = nullptr;
  multisample_renderbuffer_ = 0;
  depth_stencil_buffer_ = 0;
  premultiplied_alpha_false_texture_ = 0;
  multisample_fbo_ = 0;
  fbo_ = 0;

  if (layer_)
    GraphicsLayer::UnregisterContentsLayer(layer_.get());

  client_ = nullptr;
}

bool DrawingBuffer::ResizeDefaultFramebuffer(const IntSize& size) {
  DCHECK(state_restorer_);
  // Recreate m_backColorBuffer.
  back_color_buffer_ = CreateColorBuffer(size);

  // Most OS compositors assume GpuMemoryBuffers contain premultiplied-alpha
  // content. If the user created the context with premultipliedAlpha:false and
  // GpuMemoryBuffers are being used, allocate a non-GMB texture which will hold
  // the non-premultiplied rendering results. These will be copied into the GMB
  // via CopySubTextureCHROMIUM, performing the premultiplication step then.
  if (ShouldUseChromiumImage() && allocate_alpha_channel_ &&
      !premultiplied_alpha_) {
    state_restorer_->SetTextureBindingDirty();
    // TODO(kbr): unify with code in CreateColorBuffer.
    if (premultiplied_alpha_false_texture_) {
      gl_->DeleteTextures(1, &premultiplied_alpha_false_texture_);
      premultiplied_alpha_false_mailbox_.SetZero();
      premultiplied_alpha_false_texture_ = 0;
    }
    gl_->GenTextures(1, &premultiplied_alpha_false_texture_);
    // The command decoder forbids allocating "real" OpenGL textures with the
    // GL_TEXTURE_RECTANGLE_ARB target. Allocate this temporary texture with
    // type GL_TEXTURE_2D all the time. CopySubTextureCHROMIUM can handle
    // copying between 2D and rectangular textures.
    gl_->BindTexture(GL_TEXTURE_2D, premultiplied_alpha_false_texture_);
    if (storage_texture_supported_) {
      GLenum internal_storage_format = GL_RGBA8;
      if (use_half_float_storage_) {
        internal_storage_format = GL_RGBA16F_EXT;
      }
      gl_->TexStorage2DEXT(GL_TEXTURE_2D, 1, internal_storage_format,
                           size.Width(), size.Height());
    } else {
      GLenum internal_format = GL_RGBA;
      GLenum format = internal_format;
      GLenum data_type = GL_UNSIGNED_BYTE;
      if (use_half_float_storage_) {
        if (webgl_version_ > kWebGL1) {
          internal_format = GL_RGBA16F;
          data_type = GL_HALF_FLOAT;
        } else {
          internal_format = GL_RGBA;
          data_type = GL_HALF_FLOAT_OES;
        }
      }
      gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format, size.Width(),
                      size.Height(), 0, format, data_type, nullptr);
    }
  }

  AttachColorBufferToReadFramebuffer();

  if (WantExplicitResolve()) {
    state_restorer_->SetFramebufferBindingDirty();
    state_restorer_->SetRenderbufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, multisample_renderbuffer_);
    // Note that the multisample rendertarget will allocate an alpha channel
    // based on |have_alpha_channel_|, not |allocate_alpha_channel_|, since it
    // will resolve into the ColorBuffer.
    GLenum internal_format = have_alpha_channel_ ? GL_RGBA8_OES : GL_RGB8_OES;
    if (use_half_float_storage_) {
      DCHECK(want_alpha_channel_);
      internal_format = GL_RGBA16F_EXT;
    }
    gl_->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                                internal_format, size.Width(),
                                                size.Height());

    if (gl_->GetError() == GL_OUT_OF_MEMORY)
      return false;

    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, multisample_renderbuffer_);
  }

  if (WantDepthOrStencil()) {
    state_restorer_->SetFramebufferBindingDirty();
    state_restorer_->SetRenderbufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER,
                         multisample_fbo_ ? multisample_fbo_ : fbo_);
    if (!depth_stencil_buffer_)
      gl_->GenRenderbuffers(1, &depth_stencil_buffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer_);
    if (anti_aliasing_mode_ == gpu::kAntialiasingModeMSAAImplicitResolve) {
      gl_->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sample_count_,
                                             GL_DEPTH24_STENCIL8_OES,
                                             size.Width(), size.Height());
    } else if (anti_aliasing_mode_ ==
               gpu::kAntialiasingModeMSAAExplicitResolve) {
      gl_->RenderbufferStorageMultisampleCHROMIUM(
          GL_RENDERBUFFER, sample_count_, GL_DEPTH24_STENCIL8_OES, size.Width(),
          size.Height());
    } else {
      gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                               size.Width(), size.Height());
    }
    // For ES 2.0 contexts DEPTH_STENCIL is not available natively, so we
    // emulate
    // it at the command buffer level for WebGL contexts.
    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                 GL_RENDERBUFFER, depth_stencil_buffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  if (WantExplicitResolve()) {
    state_restorer_->SetFramebufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    if (gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      return false;
  }

  state_restorer_->SetFramebufferBindingDirty();
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  return gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void DrawingBuffer::ClearFramebuffers(GLbitfield clear_mask) {
  ScopedStateRestorer scoped_state_restorer(this);
  ClearFramebuffersInternal(clear_mask);
}

void DrawingBuffer::ClearFramebuffersInternal(GLbitfield clear_mask) {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  // We will clear the multisample FBO, but we also need to clear the
  // non-multisampled buffer.
  if (multisample_fbo_) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
  }

  gl_->BindFramebuffer(GL_FRAMEBUFFER,
                       multisample_fbo_ ? multisample_fbo_ : fbo_);
  gl_->Clear(clear_mask);
}

IntSize DrawingBuffer::AdjustSize(const IntSize& desired_size,
                                  const IntSize& cur_size,
                                  int max_texture_size) {
  IntSize adjusted_size = desired_size;

  // Clamp if the desired size is greater than the maximum texture size for the
  // device.
  if (adjusted_size.Height() > max_texture_size)
    adjusted_size.SetHeight(max_texture_size);

  if (adjusted_size.Width() > max_texture_size)
    adjusted_size.SetWidth(max_texture_size);

  return adjusted_size;
}

bool DrawingBuffer::Resize(const IntSize& new_size) {
  ScopedStateRestorer scoped_state_restorer(this);
  return ResizeFramebufferInternal(new_size);
}

bool DrawingBuffer::ResizeFramebufferInternal(const IntSize& new_size) {
  DCHECK(state_restorer_);
  DCHECK(!new_size.IsEmpty());
  IntSize adjusted_size = AdjustSize(new_size, size_, max_texture_size_);
  if (adjusted_size.IsEmpty())
    return false;

  if (adjusted_size != size_) {
    do {
      if (!ResizeDefaultFramebuffer(adjusted_size)) {
        adjusted_size.Scale(kResourceAdjustedRatio);
        continue;
      }
      break;
    } while (!adjusted_size.IsEmpty());

    size_ = adjusted_size;
    // Free all mailboxes, because they are now of the wrong size. Only the
    // first call in this loop has any effect.
    recycled_color_buffer_queue_.clear();
    recycled_bitmaps_.clear();

    if (adjusted_size.IsEmpty())
      return false;
  }

  state_restorer_->SetClearStateDirty();
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->ClearColor(0, 0, 0,
                  DefaultBufferRequiresAlphaChannelToBePreserved() ? 1 : 0);
  gl_->ColorMask(true, true, true, true);

  GLbitfield clear_mask = GL_COLOR_BUFFER_BIT;
  if (!!depth_stencil_buffer_) {
    gl_->ClearDepthf(1.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    gl_->DepthMask(true);
  }
  if (!!depth_stencil_buffer_) {
    gl_->ClearStencil(0);
    clear_mask |= GL_STENCIL_BUFFER_BIT;
    gl_->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  ClearFramebuffersInternal(clear_mask);
  return true;
}

void DrawingBuffer::ResolveAndBindForReadAndDraw() {
  {
    ScopedStateRestorer scoped_state_restorer(this);
    ResolveIfNeeded();
  }
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void DrawingBuffer::ResolveMultisampleFramebufferInternal() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  if (WantExplicitResolve()) {
    state_restorer_->SetClearStateDirty();
    gl_->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, multisample_fbo_);
    gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, fbo_);
    gl_->Disable(GL_SCISSOR_TEST);

    int width = size_.Width();
    int height = size_.Height();
    // Use NEAREST, because there is no scale performed during the blit.
    GLuint filter = GL_NEAREST;

    gl_->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                                 GL_COLOR_BUFFER_BIT, filter);

    // On old AMD GPUs on OS X, glColorMask doesn't work correctly for
    // multisampled renderbuffers and the alpha channel can be overwritten.
    // Clear the alpha channel of |m_fbo|.
    if (DefaultBufferRequiresAlphaChannelToBePreserved() &&
        ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
            gpu::DISABLE_MULTISAMPLING_COLOR_MASK_USAGE)) {
      gl_->ClearColor(0, 0, 0, 1);
      gl_->ColorMask(false, false, false, true);
      gl_->Clear(GL_COLOR_BUFFER_BIT);
    }
  }

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  if (anti_aliasing_mode_ == gpu::kAntialiasingModeScreenSpaceAntialiasing)
    gl_->ApplyScreenSpaceAntialiasingCHROMIUM();
}

void DrawingBuffer::ResolveIfNeeded() {
  if (anti_aliasing_mode_ != gpu::kAntialiasingModeNone &&
      !contents_change_resolved_)
    ResolveMultisampleFramebufferInternal();
  contents_change_resolved_ = true;
}

void DrawingBuffer::RestoreFramebufferBindings() {
  client_->DrawingBufferClientRestoreFramebufferBinding();
}

void DrawingBuffer::RestoreAllState() {
  client_->DrawingBufferClientRestoreScissorTest();
  client_->DrawingBufferClientRestoreMaskAndClearValues();
  client_->DrawingBufferClientRestorePixelPackParameters();
  client_->DrawingBufferClientRestoreTexture2DBinding();
  client_->DrawingBufferClientRestoreRenderbufferBinding();
  client_->DrawingBufferClientRestoreFramebufferBinding();
  client_->DrawingBufferClientRestorePixelUnpackBufferBinding();
  client_->DrawingBufferClientRestorePixelPackBufferBinding();
}

bool DrawingBuffer::Multisample() const {
  return anti_aliasing_mode_ != gpu::kAntialiasingModeNone;
}

void DrawingBuffer::Bind(GLenum target) {
  gl_->BindFramebuffer(target, WantExplicitResolve() ? multisample_fbo_ : fbo_);
}

scoped_refptr<Uint8Array> DrawingBuffer::PaintRenderingResultsToDataArray(
    SourceDrawingBuffer source_buffer) {
  ScopedStateRestorer scoped_state_restorer(this);

  int width = Size().Width();
  int height = Size().Height();

  base::CheckedNumeric<int> data_size = 4;
  data_size *= width;
  data_size *= height;
  if (RuntimeEnabledFeatures::CanvasColorManagementEnabled() &&
      use_half_float_storage_) {
    data_size *= 2;
  }
  if (!data_size.IsValid())
    return nullptr;

  unsigned byte_length = width * height * 4;
  if (RuntimeEnabledFeatures::CanvasColorManagementEnabled() &&
      use_half_float_storage_) {
    byte_length *= 2;
  }
  scoped_refptr<ArrayBuffer> dst_buffer =
      ArrayBuffer::CreateOrNull(byte_length, 1);
  if (!dst_buffer)
    return nullptr;
  scoped_refptr<Uint8Array> data_array =
      Uint8Array::Create(std::move(dst_buffer), 0, byte_length);
  if (!data_array)
    return nullptr;

  GLuint fbo = 0;
  state_restorer_->SetFramebufferBindingDirty();
  if (source_buffer == kFrontBuffer && front_color_buffer_) {
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, front_color_buffer_->texture_id,
                              0);
  } else {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }

  ReadBackFramebuffer(static_cast<unsigned char*>(data_array->Data()), width,
                      height, kReadbackRGBA,
                      WebGLImageConversion::kAlphaDoNothing);
  FlipVertically(static_cast<uint8_t*>(data_array->Data()), width, height);

  if (fbo) {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
  }

  return data_array;
}

void DrawingBuffer::ReadBackFramebuffer(unsigned char* pixels,
                                        int width,
                                        int height,
                                        ReadbackOrder readback_order,
                                        WebGLImageConversion::AlphaOp op) {
  DCHECK(state_restorer_);
  state_restorer_->SetPixelPackParametersDirty();
  gl_->PixelStorei(GL_PACK_ALIGNMENT, 1);
  if (webgl_version_ > kWebGL1) {
    gl_->PixelStorei(GL_PACK_SKIP_ROWS, 0);
    gl_->PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    gl_->PixelStorei(GL_PACK_ROW_LENGTH, 0);

    state_restorer_->SetPixelPackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  }

  GLenum data_type = GL_UNSIGNED_BYTE;
  if (RuntimeEnabledFeatures::CanvasColorManagementEnabled() &&
      use_half_float_storage_) {
    if (webgl_version_ > kWebGL1)
      data_type = GL_HALF_FLOAT;
    else
      data_type = GL_HALF_FLOAT_OES;
  }
  gl_->ReadPixels(0, 0, width, height, GL_RGBA, data_type, pixels);

  size_t buffer_size = 4 * width * height;
  if (data_type != GL_UNSIGNED_BYTE)
    buffer_size *= 2;
  // For half float storage Skia order is RGBA, hence no swizzling is needed.
  if (readback_order == kReadbackSkia && data_type == GL_UNSIGNED_BYTE) {
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
    // Swizzle red and blue channels to match SkBitmap's byte ordering.
    // TODO(kbr): expose GL_BGRA as extension.
    for (size_t i = 0; i < buffer_size; i += 4) {
      std::swap(pixels[i], pixels[i + 2]);
    }
#endif
  }

  if (op == WebGLImageConversion::kAlphaDoPremultiply) {
    auto color_type = kRGBA_8888_SkColorType;
    if (data_type != GL_UNSIGNED_BYTE)
      color_type = kRGBA_F16_SkColorType;
    const auto src =
        SkImageInfo::Make(width, height, color_type, kUnpremul_SkAlphaType);
    const auto dst =
        SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
    SkPixmap{src, pixels, src.minRowBytes()}.readPixels(
        SkPixmap{dst, pixels, dst.minRowBytes()});
  } else if (op != WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
  }
}

void DrawingBuffer::FlipVertically(uint8_t* framebuffer,
                                   int width,
                                   int height) {
  unsigned row_bytes = width * 4;
  if (RuntimeEnabledFeatures::CanvasColorManagementEnabled() &&
      use_half_float_storage_) {
    row_bytes *= 2;
  }
  std::vector<uint8_t> scanline(row_bytes);
  unsigned count = height / 2;
  for (unsigned i = 0; i < count; i++) {
    uint8_t* row_a = framebuffer + i * row_bytes;
    uint8_t* row_b = framebuffer + (height - i - 1) * row_bytes;
    memcpy(scanline.data(), row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline.data(), row_bytes);
  }
}

scoped_refptr<DrawingBuffer::ColorBuffer> DrawingBuffer::CreateColorBuffer(
    const IntSize& size) {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  // Select the parameters for the texture object. Allocate the backing
  // GpuMemoryBuffer and GLImage, if one is going to be used.
  GLuint image_id = 0;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (ShouldUseChromiumImage()) {
    gfx::BufferFormat buffer_format;
    GLenum gl_format = GL_NONE;
    if (allocate_alpha_channel_) {
      buffer_format = use_half_float_storage_ ? gfx::BufferFormat::RGBA_F16
                                              : gfx::BufferFormat::RGBA_8888;
      gl_format = GL_RGBA;
    } else {
      DCHECK(!use_half_float_storage_);
      buffer_format = gfx::BufferFormat::RGBX_8888;
      if (gpu::IsImageFromGpuMemoryBufferFormatSupported(
              gfx::BufferFormat::BGRX_8888,
              ContextProvider()->GetCapabilities()))
        buffer_format = gfx::BufferFormat::BGRX_8888;
      gl_format = GL_RGB;
    }
    gpu_memory_buffer = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        gfx::Size(size), buffer_format, gfx::BufferUsage::SCANOUT,
        gpu::kNullSurfaceHandle);
    if (gpu_memory_buffer) {
      gpu_memory_buffer->SetColorSpace(storage_color_space_);
      image_id =
          gl_->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                                   size.Width(), size.Height(), gl_format);
      if (!image_id)
        gpu_memory_buffer.reset();
    }
  }

  // Allocate the texture for this object.
  GLuint texture_id = 0;
  {
    gl_->GenTextures(1, &texture_id);
    gl_->BindTexture(texture_target_, texture_id);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  // If this is GpuMemoryBuffer-backed, then bind the texture to the
  // GpuMemoryBuffer's GLImage. Otherwise, allocate ordinary texture storage.
  if (image_id) {
    gl_->BindTexImage2DCHROMIUM(texture_target_, image_id);
  } else {
    if (storage_texture_supported_) {
      GLenum internal_storage_format =
          allocate_alpha_channel_ ? GL_RGBA8 : GL_RGB8;
      if (use_half_float_storage_) {
        DCHECK(want_alpha_channel_);
        internal_storage_format = GL_RGBA16F_EXT;
      }
      gl_->TexStorage2DEXT(GL_TEXTURE_2D, 1, internal_storage_format,
                           size.Width(), size.Height());
    } else {
      GLenum internal_format = allocate_alpha_channel_ ? GL_RGBA : GL_RGB;
      GLenum format = internal_format;
      GLenum data_type = GL_UNSIGNED_BYTE;
      if (use_half_float_storage_) {
        DCHECK(want_alpha_channel_);
        if (webgl_version_ > kWebGL1) {
          internal_format = GL_RGBA16F;
          data_type = GL_HALF_FLOAT;
        } else {
          internal_format = GL_RGBA;
          data_type = GL_HALF_FLOAT_OES;
        }
      }
      gl_->TexImage2D(texture_target_, 0, internal_format, size.Width(),
                      size.Height(), 0, format, data_type, nullptr);
    }
  }

  // Clear the alpha channel if this is RGB emulated.
  if (image_id && !want_alpha_channel_ && have_alpha_channel_) {
    GLuint fbo = 0;

    state_restorer_->SetClearStateDirty();
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, texture_id, 0);
    gl_->ClearColor(0, 0, 0, 1);
    gl_->ColorMask(false, false, false, true);
    gl_->Disable(GL_SCISSOR_TEST);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
  }

  return base::AdoptRef(new ColorBuffer(this, size, texture_id, image_id,
                                        std::move(gpu_memory_buffer)));
}

void DrawingBuffer::AttachColorBufferToReadFramebuffer() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  GLenum id = 0;
  GLenum texture_target = 0;

  if (premultiplied_alpha_false_texture_) {
    id = premultiplied_alpha_false_texture_;
    texture_target = GL_TEXTURE_2D;
  } else {
    id = back_color_buffer_->texture_id;
    texture_target = texture_target_;
  }

  gl_->BindTexture(texture_target, id);

  if (anti_aliasing_mode_ == gpu::kAntialiasingModeMSAAImplicitResolve) {
    gl_->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, id, 0,
        sample_count_);
  } else {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target, id, 0);
  }
}

bool DrawingBuffer::WantExplicitResolve() {
  return anti_aliasing_mode_ == gpu::kAntialiasingModeMSAAExplicitResolve;
}

bool DrawingBuffer::WantDepthOrStencil() {
  return want_depth_ || want_stencil_;
}

bool DrawingBuffer::SetupRGBEmulationForBlitFramebuffer(
    bool is_user_draw_framebuffer_bound) {
  // We only need to do this work if:
  //  - We are blitting to the default framebuffer
  //  - The user has selected alpha:false and antialias:false
  //  - We are using CHROMIUM_image with RGB emulation
  // macOS is the only platform on which this is necessary.

  if (is_user_draw_framebuffer_bound) {
    return false;
  }

  if (anti_aliasing_mode_ != gpu::kAntialiasingModeNone)
    return false;

  bool has_emulated_rgb = !allocate_alpha_channel_ && have_alpha_channel_;
  if (!has_emulated_rgb)
    return false;

  // If for some reason the back buffer doesn't exist or doesn't have a
  // CHROMIUM_image, don't proceed with this workaround.
  if (!back_color_buffer_ || !back_color_buffer_->image_id)
    return false;

  // Before allowing the BlitFramebuffer call to go through, it's necessary
  // to swap out the RGBA texture that's bound to the CHROMIUM_image
  // instance with an RGB texture. BlitFramebuffer requires the internal
  // formats of the source and destination to match when doing a
  // multisample resolve, and the best way to achieve this without adding
  // more full-screen blits is to hook up a true RGB texture to the
  // underlying IOSurface. Unfortunately, on macOS, this rendering path
  // destroys the alpha channel and requires a fixup afterward, which is
  // why it isn't used all the time.

  GLuint rgb_texture = back_color_buffer_->rgb_workaround_texture_id;
  DCHECK_EQ(texture_target_, GC3D_TEXTURE_RECTANGLE_ARB);
  if (!rgb_texture) {
    gl_->GenTextures(1, &rgb_texture);
    gl_->BindTexture(texture_target_, rgb_texture);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(texture_target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Bind this texture to the CHROMIUM_image instance that the color
    // buffer owns. This is an expensive operation, so it's important that
    // the result be cached.
    gl_->BindTexImage2DWithInternalformatCHROMIUM(texture_target_, GL_RGB,
                                                  back_color_buffer_->image_id);
    back_color_buffer_->rgb_workaround_texture_id = rgb_texture;
  }

  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER_ANGLE, GL_COLOR_ATTACHMENT0,
                            texture_target_, rgb_texture, 0);
  return true;
}

void DrawingBuffer::CleanupRGBEmulationForBlitFramebuffer() {
  // This will only be called if SetupRGBEmulationForBlitFramebuffer was.
  // Put the framebuffer back the way it was, and clear the alpha channel.
  DCHECK(back_color_buffer_);
  DCHECK(back_color_buffer_->image_id);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER_ANGLE, GL_COLOR_ATTACHMENT0,
                            texture_target_, back_color_buffer_->texture_id, 0);
  // Clear the alpha channel.
  gl_->ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->ClearColor(0, 0, 0, 1);
  gl_->Clear(GL_COLOR_BUFFER_BIT);
  DCHECK(client_);
  client_->DrawingBufferClientRestoreScissorTest();
  client_->DrawingBufferClientRestoreMaskAndClearValues();
}

DrawingBuffer::ScopedStateRestorer::ScopedStateRestorer(
    DrawingBuffer* drawing_buffer)
    : drawing_buffer_(drawing_buffer) {
  // If this is a nested restorer, save the previous restorer.
  previous_state_restorer_ = drawing_buffer->state_restorer_;
  drawing_buffer_->state_restorer_ = this;
}

DrawingBuffer::ScopedStateRestorer::~ScopedStateRestorer() {
  DCHECK_EQ(drawing_buffer_->state_restorer_, this);
  drawing_buffer_->state_restorer_ = previous_state_restorer_;
  Client* client = drawing_buffer_->client_;
  if (!client)
    return;

  if (clear_state_dirty_) {
    client->DrawingBufferClientRestoreScissorTest();
    client->DrawingBufferClientRestoreMaskAndClearValues();
  }
  if (pixel_pack_parameters_dirty_)
    client->DrawingBufferClientRestorePixelPackParameters();
  if (texture_binding_dirty_)
    client->DrawingBufferClientRestoreTexture2DBinding();
  if (renderbuffer_binding_dirty_)
    client->DrawingBufferClientRestoreRenderbufferBinding();
  if (framebuffer_binding_dirty_)
    client->DrawingBufferClientRestoreFramebufferBinding();
  if (pixel_unpack_buffer_binding_dirty_)
    client->DrawingBufferClientRestorePixelUnpackBufferBinding();
  if (pixel_pack_buffer_binding_dirty_)
    client->DrawingBufferClientRestorePixelPackBufferBinding();
}

bool DrawingBuffer::ShouldUseChromiumImage() {
  return RuntimeEnabledFeatures::WebGLImageChromiumEnabled() &&
         chromium_image_usage_ == kAllowChromiumImage &&
         Platform::Current()->GetGpuMemoryBufferManager();
}

}  // namespace blink
