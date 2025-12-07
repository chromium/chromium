// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {

class ScopedPixelLocalStorageInterrupt {
 public:
  explicit ScopedPixelLocalStorageInterrupt(
      blink::DrawingBuffer::Client* client)
      : client_(client) {
    if (client_) {
      client_->DrawingBufferClientInterruptPixelLocalStorage();
    }
  }
  ~ScopedPixelLocalStorageInterrupt() {
    if (client_) {
      client_->DrawingBufferClientRestorePixelLocalStorage();
    }
  }

 private:
  const raw_ptr<blink::DrawingBuffer::Client> client_;
};

}  // namespace

namespace blink {

// Large parts of this file have been shamelessly borrowed from
// platform/graphics/gpu/DrawingBuffer.cpp and simplified where applicable due
// to the more narrow use case. It may make sense in the future to abstract out
// some of the common bits into a base class?

XRWebGLDrawingBuffer::ColorBuffer::ColorBuffer(
    base::WeakPtr<XRWebGLDrawingBuffer> drawing_buffer,
    const gfx::Size& size,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    std::unique_ptr<gpu::SharedImageTexture> texture)
    : owning_thread_ref(base::PlatformThread::CurrentRef()),
      drawing_buffer(std::move(drawing_buffer)),
      size(size),
      shared_image(std::move(shared_image)),
      texture_(std::move(texture)) {}

void XRWebGLDrawingBuffer::ColorBuffer::BeginAccess() {
  scoped_access_ =
      texture_->BeginAccess(receive_sync_token, /*readonly=*/false);
}

void XRWebGLDrawingBuffer::ColorBuffer::EndAccess() {
  produce_sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(scoped_access_));
  shared_image->UpdateDestructionSyncToken(produce_sync_token);
}

void XRWebGLDrawingBuffer::ColorBuffer::CleanUp() {
  if (scoped_access_) {
    EndAccess();
  }
  texture_.reset();
}

scoped_refptr<XRWebGLDrawingBuffer> XRWebGLDrawingBuffer::Create(
    DrawingBuffer* drawing_buffer,
    GLuint framebuffer,
    const gfx::Size& size,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool want_antialiasing) {
  DCHECK(drawing_buffer);

  // Don't proceeed if the context is already lost.
  if (drawing_buffer->destroyed())
    return nullptr;

  gpu::gles2::GLES2Interface* gl = drawing_buffer->ContextGL();

  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util->IsValid()) {
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

  scoped_refptr<XRWebGLDrawingBuffer> xr_drawing_buffer =
      base::AdoptRef(new XRWebGLDrawingBuffer(
          drawing_buffer, framebuffer, discard_framebuffer_supported,
          want_alpha_channel, want_depth_buffer, want_stencil_buffer));
  if (!xr_drawing_buffer->Initialize(size, multisample_supported)) {
    DLOG(ERROR) << "XRWebGLDrawingBuffer Initialization Failed";
    return nullptr;
  }

  return xr_drawing_buffer;
}

XRWebGLDrawingBuffer::XRWebGLDrawingBuffer(DrawingBuffer* drawing_buffer,
                                           GLuint framebuffer,
                                           bool discard_framebuffer_supported,
                                           bool want_alpha_channel,
                                           bool want_depth_buffer,
                                           bool want_stencil_buffer)
    : drawing_buffer_(drawing_buffer),
      framebuffer_(framebuffer),
      discard_framebuffer_supported_(discard_framebuffer_supported),
      depth_(want_depth_buffer),
      stencil_(want_stencil_buffer),
      alpha_(want_alpha_channel),
      weak_factory_(this) {}

void XRWebGLDrawingBuffer::BeginDestruction() {
  if (back_color_buffer_) {
    back_color_buffer_->EndAccess();
    back_color_buffer_ = nullptr;
  }

  front_color_buffer_ = nullptr;
  recycled_color_buffer_queue_.clear();

  for (auto color_buffer : exported_color_buffers_) {
    color_buffer->CleanUp();
  }
  exported_color_buffers_.clear();
}

// TODO(bajones): The GL resources allocated in this function are leaking. Add
// a way to clean up the buffers when the layer is GCed or the session ends.
bool XRWebGLDrawingBuffer::Initialize(const gfx::Size& size,
                                      bool use_multisampling) {
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);

  gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  DVLOG(2) << __func__ << ": max_texture_size_=" << max_texture_size_;

  // Check context capabilities
  int max_sample_count = 0;
  anti_aliasing_mode_ = kNone;
  if (use_multisampling) {
    gl->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &max_sample_count);
    anti_aliasing_mode_ = kMSAAExplicitResolve;
    const auto& gpu_feature_info =
        drawing_buffer_->ContextProvider()->GetGpuFeatureInfo();
    const bool is_using_graphite =
        gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
        gpu::kGpuFeatureStatusEnabled;
    // With Graphite, Skia is not using ANGLE, so ANGLE cannot do an implicit
    // resolve when the back buffer is sampled by Skia.
    if (!is_using_graphite && extensions_util->SupportsExtension(
                                  "GL_EXT_multisampled_render_to_texture")) {
      anti_aliasing_mode_ = kMSAAImplicitResolve;
    }
  }
  DVLOG(2) << __func__
           << ": anti_aliasing_mode_=" << static_cast<int>(anti_aliasing_mode_);

#if BUILDFLAG(IS_ANDROID)
  // On Android devices use a smaller number of samples to provide more breathing
  // room for fill-rate-bound applications.
  sample_count_ = std::min(2, max_sample_count);
#else
  sample_count_ = std::min(4, max_sample_count);
#endif

  Resize(size);

  // It's possible that the drawing buffer allocation provokes a context loss,
  // so check again just in case.
  if (gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    return false;
  }

  return true;
}

gpu::gles2::GLES2Interface* XRWebGLDrawingBuffer::ContextGL() {
  return drawing_buffer_->ContextGL();
}

bool XRWebGLDrawingBuffer::ContextLost() {
  return drawing_buffer_->destroyed();
}

gfx::Size XRWebGLDrawingBuffer::AdjustSize(const gfx::Size& new_size) {
  // Ensure we always have at least a 1x1 buffer
  float width = std::max(1, new_size.width());
  float height = std::max(1, new_size.height());

  float adjusted_scale =
      std::min(static_cast<float>(max_texture_size_) / width,
               static_cast<float>(max_texture_size_) / height);

  // Clamp if the desired size is greater than the maximum texture size for the
  // device. Scale both dimensions proportionally so that we avoid stretching.
  if (adjusted_scale < 1.0f) {
    width *= adjusted_scale;
    height *= adjusted_scale;
  }

  return gfx::Size(width, height);
}

void XRWebGLDrawingBuffer::UseSharedBuffer(
    const scoped_refptr<gpu::ClientSharedImage>& buffer_shared_image,
    const gpu::SyncToken& buffer_sync_token) {
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(
      drawing_buffer_->client());
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  // Ensure that the shared image is ready to use, the following actions need
  // to be sequenced after setup steps that were done through a different
  // process's GPU command buffer context.
  //
  // TODO(https://crbug.com/1111526): Investigate handling context loss and
  // recovery for cases where these assumptions may not be accurate.
  DCHECK(buffer_sync_token.HasData());
  DCHECK(buffer_shared_image);
  DVLOG(3) << __func__
           << ": mailbox=" << buffer_shared_image->mailbox().ToDebugString()
           << ", SyncToken=" << buffer_sync_token.ToDebugString()
           << ", size=" << buffer_shared_image->size().ToString();

  // Create a texture backed by the shared buffer image.
  DCHECK(!shared_buffer_texture_);
  shared_buffer_texture_ = buffer_shared_image->CreateGLTexture(gl);
  shared_buffer_scoped_access_ =
      shared_buffer_texture_->BeginAccess(buffer_sync_token,
                                          /*readonly=*/false);

  if (WantExplicitResolve()) {
    // Bind the shared texture to the destination framebuffer of
    // the explicit resolve step.
    if (!resolved_framebuffer_) {
      gl->GenFramebuffers(1, &resolved_framebuffer_);
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, resolved_framebuffer_);
  } else {
    // Bind the shared texture directly to the drawing framebuffer.
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  }

  if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
    gl->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        shared_buffer_scoped_access_->texture_id(), 0, sample_count_);
  } else {
    // Explicit resolve, screen space antialiasing, or no antialiasing.
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D,
                             shared_buffer_scoped_access_->texture_id(), 0);
  }

  if (!framebuffer_complete_checked_for_sharedbuffer_) {
    DCHECK(gl->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
           GL_FRAMEBUFFER_COMPLETE);
    framebuffer_complete_checked_for_sharedbuffer_ = true;
  }

  if (WantExplicitResolve()) {
    // Bind the drawing framebuffer if it wasn't bound previously.
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  }

  ClearBoundFramebuffer();

  DrawingBuffer::Client* client = drawing_buffer_->client();
  if (!client)
    return;
  client->DrawingBufferClientRestoreFramebufferBinding();
}

void XRWebGLDrawingBuffer::DoneWithSharedBuffer() {
  DVLOG(3) << __func__;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(
      drawing_buffer_->client());
  BindAndResolveDestinationFramebuffer();

  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  // Discard the depth and stencil attachments since we're done with them.
  // Don't discard the color buffer, we do need this rendered into the
  // shared buffer.
  if (discard_framebuffer_supported_) {
    const GLenum kAttachments[3] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    gl->DiscardFramebufferEXT(GL_FRAMEBUFFER, 2, kAttachments);
  }

  // Always bind to the default framebuffer as a hint to the GPU to start
  // rendering now.
  gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

  // Done with the texture created by CreateAndTexStorage2DSharedImageCHROMIUM
  // finish accessing and delete it.
  DCHECK(shared_buffer_texture_);
  gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(shared_buffer_scoped_access_));
  shared_buffer_texture_.reset();

  DrawingBuffer::Client* client = drawing_buffer_->client();
  if (!client)
    return;
  client->DrawingBufferClientRestoreFramebufferBinding();
}

GLuint XRWebGLDrawingBuffer::GetCurrentColorBufferTextureId() {
  return back_color_buffer_->texture_id();
}

void XRWebGLDrawingBuffer::ClearBoundFramebuffer() {
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(
      drawing_buffer_->client());
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  GLbitfield clear_bits = GL_COLOR_BUFFER_BIT;
  gl->ColorMask(true, true, true, true);
  gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  if (depth_) {
    clear_bits |= GL_DEPTH_BUFFER_BIT;
    gl->DepthMask(true);
    gl->ClearDepthf(1.0f);
  }

  if (stencil_) {
    clear_bits |= GL_STENCIL_BUFFER_BIT;
    gl->StencilMaskSeparate(GL_FRONT, true);
    gl->ClearStencil(0);
  }

  gl->Disable(GL_SCISSOR_TEST);

  gl->Clear(clear_bits);

  DrawingBuffer::Client* client = drawing_buffer_->client();
  if (!client)
    return;

  client->DrawingBufferClientRestoreScissorTest();
  client->DrawingBufferClientRestoreMaskAndClearValues();
}

void XRWebGLDrawingBuffer::Resize(const gfx::Size& new_size) {
  gfx::Size adjusted_size = AdjustSize(new_size);

  if (adjusted_size == size_)
    return;

  // Don't attempt to resize if the context is lost.
  if (ContextLost())
    return;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(
      drawing_buffer_->client());
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  size_ = adjusted_size;

  // Free all mailboxes, because they are now of the wrong size. Only the
  // first call in this loop has any effect.
  recycled_color_buffer_queue_.clear();

  gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

  // Provide a depth and/or stencil buffer if requested.
  if (depth_ || stencil_) {
    if (depth_stencil_buffer_) {
      gl->DeleteRenderbuffers(1, &depth_stencil_buffer_);
      depth_stencil_buffer_ = 0;
    }
    gl->GenRenderbuffers(1, &depth_stencil_buffer_);
    gl->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer_);

    if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
      gl->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sample_count_,
                                            GL_DEPTH24_STENCIL8_OES,
                                            size_.width(), size_.height());
    } else if (anti_aliasing_mode_ == kMSAAExplicitResolve) {
      gl->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                                 GL_DEPTH24_STENCIL8_OES,
                                                 size_.width(), size_.height());
    } else {
      gl->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                              size_.width(), size_.height());
    }

    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, depth_stencil_buffer_);
  }

  if (WantExplicitResolve()) {
    // If we're doing an explicit multisample resolve use the main framebuffer
    // as the multisample target and resolve into resolved_fbo_ when needed.
    GLenum multisample_format = alpha_ ? GL_RGBA8_OES : GL_RGB8_OES;

    if (multisample_renderbuffer_) {
      gl->DeleteRenderbuffers(1, &multisample_renderbuffer_);
      multisample_renderbuffer_ = 0;
    }

    gl->GenRenderbuffers(1, &multisample_renderbuffer_);
    gl->BindRenderbuffer(GL_RENDERBUFFER, multisample_renderbuffer_);
    gl->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                               multisample_format,
                                               size_.width(), size_.height());

    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, multisample_renderbuffer_);

    // Now bind the resolve target framebuffer to attach the color textures to.
    if (!resolved_framebuffer_) {
      gl->GenFramebuffers(1, &resolved_framebuffer_);
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, resolved_framebuffer_);
  }

  if (back_color_buffer_) {
    back_color_buffer_->EndAccess();
  }

  back_color_buffer_ = CreateColorBuffer();
  front_color_buffer_ = nullptr;

  back_color_buffer_->BeginAccess();

  if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
    gl->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        back_color_buffer_->texture_id(), 0, sample_count_);
  } else {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, back_color_buffer_->texture_id(),
                             0);
  }

  if (!framebuffer_complete_checked_for_resize_) {
    DCHECK(gl->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
           GL_FRAMEBUFFER_COMPLETE);
    framebuffer_complete_checked_for_resize_ = true;
  }

  DrawingBuffer::Client* client = drawing_buffer_->client();
  client->DrawingBufferClientRestoreRenderbufferBinding();
  client->DrawingBufferClientRestoreFramebufferBinding();
}

scoped_refptr<XRWebGLDrawingBuffer::ColorBuffer>
XRWebGLDrawingBuffer::CreateColorBuffer() {
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(
      drawing_buffer_->client());
  auto* sii = drawing_buffer_->ContextProvider()->SharedImageInterface();

  // These shared images will be imported into textures on the GL context. We
  // take a read/write access scope whenever the color buffer is used as the
  // back buffer.
  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                   gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;
  auto client_shared_image = sii->CreateSharedImage(
      {alpha_ ? viz::SinglePlaneFormat::kRGBA_8888
              : viz::SinglePlaneFormat::kRGBX_8888,
       size_, gfx::ColorSpace(), usage, "XRWebGLDrawingBuffer"},
      gpu::kNullSurfaceHandle);
  CHECK(client_shared_image);

  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  std::unique_ptr<gpu::SharedImageTexture> texture =
      client_shared_image->CreateGLTexture(gl);

  DrawingBuffer::Client* client = drawing_buffer_->client();
  client->DrawingBufferClientRestoreTexture2DBinding();

  return base::MakeRefCounted<ColorBuffer>(weak_factory_.GetWeakPtr(), size_,
                                           std::move(client_shared_image),
                                           std::move(texture));
}

scoped_refptr<XRWebGLDrawingBuffer::ColorBuffer>
XRWebGLDrawingBuffer::CreateOrRecycleColorBuffer() {
  if (!recycled_color_buffer_queue_.empty()) {
    scoped_refptr<ColorBuffer> recycled =
        recycled_color_buffer_queue_.TakeLast();
    DCHECK(recycled->size == size_);
    return recycled;
  }
  return CreateColorBuffer();
}

bool XRWebGLDrawingBuffer::WantExplicitResolve() const {
  return anti_aliasing_mode_ == kMSAAExplicitResolve;
}

void XRWebGLDrawingBuffer::BindAndResolveDestinationFramebuffer() {
  // Ensure that the mode-appropriate destination framebuffer's color
  // attachment contains the drawn content after any antialiasing steps needed.

  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  DrawingBuffer::Client* client = drawing_buffer_->client();
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(client);

  // Resolve multisample buffers if needed
  if (WantExplicitResolve()) {
    DVLOG(3) << __func__ << ": explicit resolve";
    gl->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, framebuffer_);
    gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, resolved_framebuffer_);
    gl->Disable(GL_SCISSOR_TEST);

    int width = size_.width();
    int height = size_.height();
    // Use NEAREST, because there is no scale performed during the blit.
    gl->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                                GL_COLOR_BUFFER_BIT, GL_NEAREST);

    gl->BindFramebuffer(GL_FRAMEBUFFER, resolved_framebuffer_);

    client->DrawingBufferClientRestoreScissorTest();
  } else {
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    DVLOG(3) << __func__ << ": nothing to do";
  }

  // On exit, leaves the destination framebuffer active. Caller is responsible
  // for restoring client bindings.
}

// Swap the front and back buffers. After this call the front buffer should
// contain the previously rendered content, resolved from the multisample
// renderbuffer if needed.
void XRWebGLDrawingBuffer::SwapColorBuffers() {
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  DrawingBuffer::Client* client = drawing_buffer_->client();
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(client);

  BindAndResolveDestinationFramebuffer();

  if (back_color_buffer_) {
    back_color_buffer_->EndAccess();
  }

  // Swap buffers
  front_color_buffer_ = back_color_buffer_;
  back_color_buffer_ = CreateOrRecycleColorBuffer();

  back_color_buffer_->BeginAccess();

  if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
    gl->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        back_color_buffer_->texture_id(), 0, sample_count_);
  } else {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, back_color_buffer_->texture_id(),
                             0);
  }

  if (!framebuffer_complete_checked_for_swap_) {
    DCHECK(gl->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
           GL_FRAMEBUFFER_COMPLETE);
    framebuffer_complete_checked_for_swap_ = true;
  }

  if (WantExplicitResolve()) {
    // Bind the drawing framebuffer if it wasn't bound previously.
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  }

  ClearBoundFramebuffer();

  client->DrawingBufferClientRestoreFramebufferBinding();
}

scoped_refptr<StaticBitmapImage>
XRWebGLDrawingBuffer::TransferToStaticBitmapImage() {
  scoped_refptr<ColorBuffer> buffer;
  bool success = false;

  // Ensure the context isn't lost and the framebuffer is complete before
  // continuing.
  if (!ContextLost()) {
    SwapColorBuffers();

    buffer = front_color_buffer_;

    // This should only fail if the context is lost during the buffer swap.
    if (buffer->produce_sync_token.HasData()) {
      success = true;
    }
  }

  if (!success) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, if the framebuffer is
    // incomplete (likely due to a failed buffer allocation), or when the
    // context gets lost.
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(size_.width(), size_.height()));
    return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
  }

  // This holds a ref on the XRWebGLDrawingBuffer that will keep it alive
  // until the mailbox is released (and while the callback is running).
  viz::ReleaseCallback release_callback =
      base::BindOnce(&XRWebGLDrawingBuffer::NotifyMailboxReleased, buffer);
  exported_color_buffers_.insert(buffer);

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      buffer->shared_image, buffer->produce_sync_token,
      buffer->shared_image->alpha_type(),
      drawing_buffer_->ContextProviderWeakPtr(),
      base::PlatformThread::CurrentRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback));
}

// static
void XRWebGLDrawingBuffer::NotifyMailboxReleased(
    scoped_refptr<ColorBuffer> color_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(color_buffer->owning_thread_ref == base::PlatformThread::CurrentRef());

  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  color_buffer->receive_sync_token = sync_token;
  color_buffer->shared_image->UpdateDestructionSyncToken(sync_token);
  if (color_buffer->drawing_buffer) {
    color_buffer->drawing_buffer->MailboxReleased(color_buffer, lost_resource);
  }
}

void XRWebGLDrawingBuffer::MailboxReleased(
    scoped_refptr<ColorBuffer> color_buffer,
    bool lost_resource) {
  // If the mailbox has been returned by the compositor then it is no
  // longer being presented, and so is no longer the front buffer.
  if (color_buffer == front_color_buffer_)
    front_color_buffer_ = nullptr;

  if (drawing_buffer_->destroyed() || color_buffer->size != size_ ||
      lost_resource) {
    return;
  }

  const size_t cache_limit = 2;
  while (recycled_color_buffer_queue_.size() >= cache_limit)
    recycled_color_buffer_queue_.TakeLast();

  recycled_color_buffer_queue_.push_front(color_buffer);
}

}  // namespace blink
