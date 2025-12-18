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

#include "base/byte_size.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/ostream_operators.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/video_frame.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Controls whether the canvas resource in ExportLowLatencyCanvasResource()
// should be created with the SyncToken returned from back color buffer
// (when enabled) or with an empty SyncToken (when disabled). Enabling this
// feature would prevent flickering in some cases where desynchronized canvas
// are periodically refreshed on Windows.
BASE_FEATURE(kUseNonEmptySyncTokenForLowLatencyCanvas,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const float kResourceAdjustedRatio = 0.5;

bool g_should_fail_drawing_buffer_creation_for_testing = false;

void FlipVertically(base::span<uint8_t> framebuffer,
                    size_t num_rows,
                    size_t row_bytes) {
  DCHECK_EQ(framebuffer.size(), num_rows * row_bytes);
  std::vector<uint8_t> swap_storage(row_bytes);
  base::span<uint8_t> row_c(swap_storage);
  for (size_t a = 0; a < num_rows / 2; a++) {
    const size_t b = num_rows - a - 1;
    auto row_a = framebuffer.subspan(a * row_bytes, row_bytes);
    auto row_b = framebuffer.subspan(b * row_bytes, row_bytes);

    // Swap vertically opposite rows.
    row_c.copy_from(row_b);
    row_b.copy_from(row_a);
    row_a.copy_from(row_c);
  }
}

sk_sp<SkData> TryAllocateSkDataForBitmap(viz::SharedImageFormat format,
                                         gfx::Size size) {
  auto data_size = format.MaybeEstimatedSizeInBytes(size);
  if (!data_size) {
    return nullptr;
  }

  return TryAllocateSkData(data_size.value());
}

class ScopedDrawBuffer {
  STACK_ALLOCATED();

 public:
  explicit ScopedDrawBuffer(gpu::gles2::GLES2Interface* gl,
                            GLenum prev_draw_buffer,
                            GLenum new_draw_buffer)
      : gl_(gl),
        prev_draw_buffer_(prev_draw_buffer),
        new_draw_buffer_(new_draw_buffer) {
    if (prev_draw_buffer_ != new_draw_buffer_) {
      gl_->DrawBuffersEXT(1, &new_draw_buffer_);
    }
  }

  ~ScopedDrawBuffer() {
    if (prev_draw_buffer_ != new_draw_buffer_) {
      gl_->DrawBuffersEXT(1, &prev_draw_buffer_);
    }
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
  GLenum prev_draw_buffer_;
  GLenum new_draw_buffer_;
};

}  // namespace

// Increase cache to avoid reallocation on fuchsia, see
// https://crbug.com/1087941.
#if BUILDFLAG(IS_FUCHSIA)
const size_t DrawingBuffer::kDefaultColorBufferCacheLimit = 2;
#else
const size_t DrawingBuffer::kDefaultColorBufferCacheLimit = 1;
#endif

// Function defined in third_party/blink/public/web/blink.h.
void ForceNextDrawingBufferCreationToFailForTest() {
  g_should_fail_drawing_buffer_creation_for_testing = true;
}

scoped_refptr<DrawingBuffer> DrawingBuffer::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::WebGLContextInfo& context_info,
    Client* client,
    const gfx::Size& size,
    bool premultiplied_alpha,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool want_antialiasing,
    bool desynchronized,
    PreserveDrawingBuffer preserve,
    Platform::WebGLContextType webgl_version,
    ChromiumImageUsage chromium_image_usage,
    PredefinedColorSpace color_space,
    gl::GpuPreference gpu_preference) {
  if (g_should_fail_drawing_buffer_creation_for_testing) {
    g_should_fail_drawing_buffer_creation_for_testing = false;
    return nullptr;
  }

  base::CheckedNumeric<int> data_size =
      SkColorTypeBytesPerPixel(kRGBA_8888_SkColorType);
  data_size *= size.width();
  data_size *= size.height();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxByteLength) {
    return nullptr;
  }

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

  bool texture_storage_enabled =
      extensions_util->IsExtensionEnabled("GL_EXT_texture_storage");

  scoped_refptr<DrawingBuffer> drawing_buffer =
      base::AdoptRef(new DrawingBuffer(
          std::move(context_provider), context_info, desynchronized,
          std::move(extensions_util), client, discard_framebuffer_supported,
          texture_storage_enabled, want_alpha_channel, premultiplied_alpha,
          preserve, webgl_version, want_depth_buffer, want_stencil_buffer,
          chromium_image_usage, color_space, gpu_preference));
  if (!drawing_buffer->Initialize(size, multisample_supported)) {
    drawing_buffer->BeginDestruction();
    return scoped_refptr<DrawingBuffer>();
  }
  return drawing_buffer;
}

DrawingBuffer::DrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::WebGLContextInfo& context_info,
    bool desynchronized,
    std::unique_ptr<Extensions3DUtil> extensions_util,
    Client* client,
    bool discard_framebuffer_supported,
    bool texture_storage_enabled,
    bool want_alpha_channel,
    bool premultiplied_alpha,
    PreserveDrawingBuffer preserve,
    Platform::WebGLContextType webgl_version,
    bool want_depth,
    bool want_stencil,
    ChromiumImageUsage chromium_image_usage,
    PredefinedColorSpace color_space,
    gl::GpuPreference gpu_preference)
    : client_(client),
      preserve_drawing_buffer_(preserve),
      webgl_version_(webgl_version),
      context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      gl_(ContextProvider()->ContextGL()),
      extensions_util_(std::move(extensions_util)),
      discard_framebuffer_supported_(discard_framebuffer_supported),
      texture_storage_enabled_(texture_storage_enabled),
      requested_alpha_type_(want_alpha_channel
                                ? (premultiplied_alpha ? kPremul_SkAlphaType
                                                       : kUnpremul_SkAlphaType)
                                : kOpaque_SkAlphaType),
      requested_format_(want_alpha_channel ? GL_RGBA8 : GL_RGB8),
      context_info_(context_info),
      using_swap_chain_(ContextProvider()
                            ->SharedImageInterface()
                            ->GetCapabilities()
                            .shared_image_swap_chain &&
                        desynchronized),
      low_latency_enabled_(desynchronized),
      want_depth_(want_depth),
      want_stencil_(want_stencil),
      color_space_(PredefinedColorSpaceToGfxColorSpace(color_space)),
      chromium_image_usage_(chromium_image_usage),
      opengl_flip_y_extension_(
          ContextProvider()->GetCapabilities().mesa_framebuffer_flip_y),
      initial_gpu_(gpu_preference),
      current_active_gpu_(gpu_preference),
      weak_factory_(this) {
  // Used by browser tests to detect the use of a DrawingBuffer.
  TRACE_EVENT_INSTANT0("test_gpu", "DrawingBufferCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
  // PowerPreferenceToGpuPreference should have resolved the meaning
  // of the "default" GPU already.
  DCHECK(gpu_preference != gl::GpuPreference::kDefault);
}

DrawingBuffer::~DrawingBuffer() {
  DCHECK(destruction_in_progress_);
  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }

  for (auto& color_buffer : exported_color_buffers_) {
    color_buffer->ForceCleanUp();
  }
  context_provider_ = nullptr;
}

bool DrawingBuffer::MarkContentsChanged() {
  if (contents_change_resolved_ || !contents_changed_) {
    contents_change_resolved_ = false;
    transient_framebuffers_discarded_ = false;
    contents_changed_ = true;
    return true;
  }
  return false;
}

bool DrawingBuffer::BufferClearNeeded() const {
  return buffer_clear_needed_;
}

void DrawingBuffer::SetBufferClearNeeded(bool flag) {
  if (preserve_drawing_buffer_ == kDiscard) {
    buffer_clear_needed_ = flag;
  } else {
    DCHECK(!buffer_clear_needed_);
  }
}

gpu::gles2::GLES2Interface* DrawingBuffer::ContextGL() {
  return gl_;
}

WebGraphicsContext3DProvider* DrawingBuffer::ContextProvider() {
  return &(context_provider_->ContextProvider());
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DrawingBuffer::ContextProviderWeakPtr() {
  return context_provider_->GetWeakPtr();
}

void DrawingBuffer::SetIsInHiddenPage(bool hidden) {
  if (is_hidden_ == hidden)
    return;
  is_hidden_ = hidden;
  if (is_hidden_) {
    recycled_color_buffer_queue_.clear();
    recycled_software_resources_.clear();
  }

  // Make sure to interrupt pixel local storage.
  ScopedStateRestorer scoped_state_restorer(this);

  auto* context_support = ContextProvider()->ContextSupport();
  if (context_support) {
    context_support->SetAggressivelyFreeResources(hidden);
  }

  gl_->ContextVisibilityHintCHROMIUM(is_hidden_ ? GL_FALSE : GL_TRUE);
  gl_->Flush();
}

void DrawingBuffer::SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata) {
  hdr_metadata_ = hdr_metadata;
}

bool DrawingBuffer::RequiresAlphaChannelToBePreserved() {
  return client_->DrawingBufferClientIsBoundForDraw() &&
         DefaultBufferRequiresAlphaChannelToBePreserved();
}

bool DrawingBuffer::DefaultBufferRequiresAlphaChannelToBePreserved() {
  return requested_alpha_type_ == kOpaque_SkAlphaType &&
         color_buffer_format_.HasAlpha();
}

void DrawingBuffer::SetDrawBuffer(GLenum draw_buffer) {
  draw_buffer_ = draw_buffer;
}

void DrawingBuffer::SetSharedImageInterfaceProviderForSoftwareRenderingTest(
    std::unique_ptr<WebGraphicsSharedImageInterfaceProvider> sii_provider) {
  shared_image_interface_provider_for_bitmap_test_ = std::move(sii_provider);
}

WebGraphicsSharedImageInterfaceProvider*
DrawingBuffer::GetSharedImageInterfaceProviderForBitmap() {
  if (shared_image_interface_provider_for_bitmap_test_) {
    return shared_image_interface_provider_for_bitmap_test_.get();
  }
  return SharedGpuContext::SharedImageInterfaceProvider();
}

DrawingBuffer::SoftwareResource
DrawingBuffer::CreateOrRecycleSoftwareResource() {
  const viz::SharedImageFormat format = viz::SinglePlaneFormat::kBGRA_8888;
  const gfx::ColorSpace& color_space =
      back_color_buffer_->shared_image->color_space();
  // Must call GetSharedImageInterfaceProvider first so all base::WeakPtr
  // restored in |resource.sii_provider| is updated.
  auto* sii_provider = GetSharedImageInterfaceProviderForBitmap();

  auto it = std::remove_if(
      recycled_software_resources_.begin(), recycled_software_resources_.end(),
      [&](const SoftwareResource& resource) {
        return resource.shared_image->size() != size_ ||
               resource.shared_image->color_space() != color_space ||
               !resource.sii_provider;
      });
  recycled_software_resources_.Shrink(
      static_cast<wtf_size_t>(it - recycled_software_resources_.begin()));

  if (!recycled_software_resources_.empty()) {
    SoftwareResource recycled = std::move(recycled_software_resources_.back());
    recycled_software_resources_.pop_back();
    return recycled;
  }

  // There are no resources to recycle so allocate a new one.
  auto* shared_image_interface = sii_provider->SharedImageInterface();
  if (!shared_image_interface) {
    return SoftwareResource();
  }
  // glReadPixels always read with bottom-Left origin regardless of framebuffer
  // flip extension, so keep shared image the same so we don't need to flip
  // here.
  auto shared_image =
      shared_image_interface->CreateSharedImageForSoftwareCompositor(
          {format, size_, color_space, kBottomLeft_GrSurfaceOrigin,
           kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
           "DrawingBufferBitmap"});

  SoftwareResource resource = {std::move(shared_image),
                               shared_image_interface->GenVerifiedSyncToken(),
                               sii_provider->GetWeakPtr()};

  return resource;
}

bool DrawingBuffer::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  ScopedStateRestorer scoped_state_restorer(this);

  if (CheckForDestructionAndChangeAndResolveIfNeeded(kDiscardAllowed) !=
      kContentsResolvedIfNeeded) {
    return false;
  }

  if (IsUsingGpuCompositing()) {
    gpu::SyncToken sync_token;
    auto shared_image =
        ExportSharedImageFromBackBuffer(sync_token, out_release_callback);
    if (!shared_image) {
      return false;
    }

    // Populate the output TransferableResource from the SharedImage.
    *out_resource = viz::TransferableResource::Make(
        shared_image, viz::TransferableResource::ResourceSource::kDrawingBuffer,
        sync_token);
    out_resource->hdr_metadata = hdr_metadata_;
    out_resource->is_low_latency_rendering = shared_image->usage().Has(
        gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
  } else {
    // Populate the TransferableResource with a SharedImage for the software
    // compositor.
    SoftwareResource resource = CreateOrRecycleSoftwareResource();
    if (!resource.shared_image) {
      return false;
    }

    auto mapping = resource.shared_image->Map();

    ReadBackFramebuffer(mapping->GetMemoryForPlane(0),
                        resource.shared_image->format(), kPremul_SkAlphaType,
                        kBottomLeft_GrSurfaceOrigin, kBackBuffer);

    *out_resource = viz::TransferableResource::Make(
        resource.shared_image,
        viz::TransferableResource::ResourceSource::kDrawingBuffer,
        resource.sync_token);

    out_resource->hdr_metadata = hdr_metadata_;
    out_resource->is_low_latency_rendering = resource.shared_image->usage().Has(
        gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);

    // This holds a ref on the DrawingBuffer that will keep it alive until the
    // mailbox is released (and while the release callback is running). It also
    // owns the resource.
    *out_release_callback =
        base::BindOnce(&DrawingBuffer::MailboxReleasedSoftware,
                       weak_factory_.GetWeakPtr(), std::move(resource));

    contents_changed_ = false;
    if (preserve_drawing_buffer_ == kDiscard) {
      SetBufferClearNeeded(true);
    }
  }

  return true;
}

DrawingBuffer::CheckForDestructionResult
DrawingBuffer::CheckForDestructionAndChangeAndResolveIfNeeded(
    DiscardBehavior discardBehavior) {
  DCHECK(state_restorer_);
  if (destruction_in_progress_) {
    // It can be hit in the following sequence.
    // 1. WebGL draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context lost using WEBGL_lose_context extension.
    // 4. Here.
    return kDestroyedOrLost;
  }

  // There used to be a DCHECK(!is_hidden_) here, but in some tab
  // switching scenarios, it seems that this can racily be called for
  // backgrounded tabs.

  if (!contents_changed_)
    return kContentsUnchanged;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return kDestroyedOrLost;

  TRACE_EVENT0("blink,rail", "DrawingBuffer::prepareMailbox");

  // Resolve the multisampled buffer into the texture attached to fbo_.
  ResolveIfNeeded(discardBehavior);

  return kContentsResolvedIfNeeded;
}

scoped_refptr<gpu::ClientSharedImage>
DrawingBuffer::ExportSharedImageFromBackBuffer(
    gpu::SyncToken& sync_token,
    viz::ReleaseCallback* out_release_callback) {
  DCHECK(state_restorer_);
  if (webgl_version_ != Platform::kWebGL1ContextType) {
    state_restorer_->SetPixelUnpackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  CopyStagingTextureToBackColorBufferIfNeeded();

  // Specify the buffer that we will put in the mailbox.
  scoped_refptr<ColorBuffer> color_buffer_for_mailbox;
  if (preserve_drawing_buffer_ == kDiscard) {
    // Send the old backbuffer directly into the mailbox, and allocate
    // (or recycle) a new backbuffer.
    color_buffer_for_mailbox = back_color_buffer_;
    back_color_buffer_ = CreateOrRecycleColorBuffer();
    if (!back_color_buffer_) {
      // Context is likely lost.
      return nullptr;
    }
    AttachColorBufferToReadFramebuffer();

    // Explicitly specify that m_fbo (which is now bound to the just-allocated
    // m_backColorBuffer) is not initialized, to save GPU memory bandwidth on
    // tile-based GPU architectures. Note that the depth and stencil attachments
    // are also discarded before multisample resolves, implicit or explicit.
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
    // TODO(sunnyps): We can skip this test if explicit resolve is used since
    // we'll render to the multisample fbo which will be preserved.
    color_buffer_for_mailbox = CreateOrRecycleColorBuffer();
    if (!color_buffer_for_mailbox) {
      // Context is likely lost.
      return nullptr;
    }
    gl_->CopySubTextureCHROMIUM(
        back_color_buffer_->texture_id(), 0,
        color_buffer_for_mailbox->shared_image->GetTextureTarget(),
        color_buffer_for_mailbox->texture_id(), 0, 0, 0, 0, 0, size_.width(),
        size_.height(), GL_FALSE, GL_FALSE, GL_FALSE);
  }

  // Signal we will no longer access |color_buffer_for_mailbox| before exporting
  // it.
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
    color_buffer_for_mailbox->produce_sync_token =
        color_buffer_for_mailbox->EndAccess();
    sync_token = color_buffer_for_mailbox->produce_sync_token;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    // Needed for GPU back-pressure on macOS and Android. Used to be in the
    // middle of the commands above; try to move it to the bottom to allow them
    // to be treated atomically.
    gl_->DescheduleUntilFinishedCHROMIUM();
#endif
  }

  // Populate the output callback.
  {
    // This holds a ref on the DrawingBuffer that will keep it alive until the
    // mailbox is released (and while the release callback is running).
    auto func = base::BindOnce(&DrawingBuffer::NotifyMailboxReleasedGpu,
                               color_buffer_for_mailbox);
    exported_color_buffers_.insert(color_buffer_for_mailbox);
    *out_release_callback = std::move(func);
  }

  // Point |m_frontColorBuffer| to the buffer that we are now presenting.
  front_color_buffer_ = color_buffer_for_mailbox;

  contents_changed_ = false;
  if (preserve_drawing_buffer_ == kDiscard) {
    SetBufferClearNeeded(true);
  }

  return color_buffer_for_mailbox->shared_image;
}

// static
void DrawingBuffer::NotifyMailboxReleasedGpu(
    scoped_refptr<ColorBuffer> color_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(color_buffer->owning_thread_ref == base::PlatformThread::CurrentRef());

  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  color_buffer->receive_sync_token = sync_token;
  if (color_buffer->drawing_buffer) {
    color_buffer->drawing_buffer->MailboxReleasedGpu(color_buffer,
                                                     lost_resource);
  }
}

void DrawingBuffer::MailboxReleasedGpu(scoped_refptr<ColorBuffer> color_buffer,
                                       bool lost_resource) {
  exported_color_buffers_.erase(color_buffer);

  // If the mailbox has been returned by the compositor then it is no
  // longer being presented, and so is no longer the front buffer.
  if (color_buffer == front_color_buffer_)
    front_color_buffer_ = nullptr;

  if (destruction_in_progress_ || color_buffer->shared_image->size() != size_ ||
      color_buffer->shared_image->format() != color_buffer_format_ ||
      color_buffer->shared_image->color_space() != color_space_ ||
      gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR || lost_resource ||
      is_hidden_) {
    return;
  }

  // Creation of image backed mailboxes is very expensive, so be less
  // aggressive about pruning them. Pruning is done in FIFO order.
  size_t cache_limit = kDefaultColorBufferCacheLimit;
  if (color_buffer->shared_image->usage().Has(
          gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
    cache_limit = 4;
  }
  while (recycled_color_buffer_queue_.size() >= cache_limit)
    recycled_color_buffer_queue_.TakeLast();

  recycled_color_buffer_queue_.push_front(color_buffer);
}

void DrawingBuffer::MailboxReleasedSoftware(SoftwareResource resource,
                                            const gpu::SyncToken& sync_token,
                                            bool lost_resource) {
  if (destruction_in_progress_ || lost_resource || is_hidden_ ||
      resource.shared_image->size() != size_) {
    // Just delete the SoftwareResource.
    return;
  }

  recycled_software_resources_.push_back(std::move(resource));
}

scoped_refptr<StaticBitmapImage> DrawingBuffer::TransferToStaticBitmapImage() {
  ScopedStateRestorer scoped_state_restorer(this);

  gpu::SyncToken sync_token;
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  viz::ReleaseCallback release_callback;

  if (CheckForDestructionAndChangeAndResolveIfNeeded(kDiscardAllowed) ==
      kContentsResolvedIfNeeded) {
    // NOTE: GPU compositing is always used on Android and ChromeOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
    shared_image =
        ExportSharedImageFromBackBuffer(sync_token, &release_callback);
#else
    if (IsUsingGpuCompositing()) {
      shared_image =
          ExportSharedImageFromBackBuffer(sync_token, &release_callback);
    } else {
      // Read back the contents of the buffer into an unaccelerated image. It's
      // necessary to do the readback here via the WebGL context as the image
      // returned from this method may later be sent to the compositor, and the
      // shared GPU context is unavailable when software compositing is used.
      auto image = GetRGBAUnacceleratedStaticBitmapImage(kBackBuffer);

      // transferToImageBitmap() semantically "takes" the image from the back
      // buffer, analogously to presentation. Hence, mark the buffer as being
      // unchanged since its last export as well as needing clearing if relevant
      // (note: for GPU compositing this is done in
      // ExportSharedImageFromBackBuffer()). This is crucial to ensure correct
      // semantics of followup operations (e.g., for offscreen canvas it's
      // needed to ensure that PushFrame() will actually push a frame after a
      // transferToImageBitmap() call).
      contents_changed_ = false;
      if (preserve_drawing_buffer_ == kDiscard) {
        SetBufferClearNeeded(true);
      }
      return image;
    }
#endif
  }

  if (!shared_image) {
    // If we couldn't resolve the contents or (for the GPU compositor) couldn't
    // produce a SharedImage out of them, return an transparent black
    // ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost. We intentionally leave the transparent black image in legacy color
    // space.
    SkBitmap black_bitmap;
    if (!black_bitmap.tryAllocN32Pixels(size_.width(), size_.height()))
      return nullptr;
    black_bitmap.eraseARGB(0, 0, 0, 0);

    // Mark the bitmap as immutable to avoid an unnecessary copy in the
    // following RasterFromBitmap() call.
    black_bitmap.setImmutable();
    sk_sp<SkImage> black_image = SkImages::RasterFromBitmap(black_bitmap);
    if (!black_image)
      return nullptr;
    return UnacceleratedStaticBitmapImage::Create(black_image);
  }

  DCHECK(release_callback);

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      std::move(shared_image), sync_token, kPremul_SkAlphaType,
      context_provider_->GetWeakPtr(), base::PlatformThread::CurrentRef(),
      ThreadScheduler::Current()->CleanupTaskRunner(),
      std::move(release_callback));
}

scoped_refptr<DrawingBuffer::ColorBuffer>
DrawingBuffer::CreateOrRecycleColorBuffer() {
  DCHECK(state_restorer_);
  if (!recycled_color_buffer_queue_.empty()) {
    scoped_refptr<ColorBuffer> recycled =
        recycled_color_buffer_queue_.TakeLast();
    DCHECK(recycled->shared_image->size() == size_);
    DCHECK(recycled->shared_image->color_space() == color_space_);
    recycled->BeginAccess(recycled->receive_sync_token, /*readonly=*/false);
    return recycled;
  }
  return CreateColorBuffer(size_);
}

scoped_refptr<ExternalCanvasResource>
DrawingBuffer::ExportLowLatencyCanvasResource() {
  gpu::SyncToken sync_token;
  if (contents_changed_) {
    ScopedStateRestorer scoped_state_restorer(this);
    ResolveIfNeeded(kDiscardAllowed);

    // Restart SharedImage access on the back buffer to ensure a write fence is
    // generated on it to guarantee display reads this frame completely.
    // Display may still read parts of subsequent frames, which is okay.
    if (base::FeatureList::IsEnabled(
            kUseNonEmptySyncTokenForLowLatencyCanvas)) {
      sync_token = back_color_buffer_->EndAccess();
    } else {
      back_color_buffer_->EndAccess();
    }
    back_color_buffer_->BeginAccess(gpu::SyncToken(), /*readonly=*/false);
  }

  return ExternalCanvasResource::Create(
      back_color_buffer_->shared_image, sync_token,
      viz::TransferableResource::ResourceSource::kDrawingBuffer, hdr_metadata_,
      viz::ReleaseCallback(), context_provider_->GetWeakPtr());
}

scoped_refptr<CanvasResource> DrawingBuffer::ExportCanvasResource() {
  ScopedStateRestorer scoped_state_restorer(this);
  TRACE_EVENT0("blink", "DrawingBuffer::ExportCanvasResource");

  if (CheckForDestructionAndChangeAndResolveIfNeeded(kDiscardAllowed) !=
      kContentsResolvedIfNeeded) {
    return nullptr;
  }

  CHECK(IsUsingGpuCompositing());
  gpu::SyncToken sync_token;
  viz::ReleaseCallback out_release_callback;
  scoped_refptr<gpu::ClientSharedImage> client_si =
      ExportSharedImageFromBackBuffer(sync_token, &out_release_callback);
  if (!client_si) {
    return nullptr;
  }

  return ExternalCanvasResource::Create(
      client_si, sync_token,
      viz::TransferableResource::ResourceSource::kDrawingBuffer, hdr_metadata_,
      std::move(out_release_callback), context_provider_->GetWeakPtr());
}

DrawingBuffer::ColorBuffer::ColorBuffer(
    base::WeakPtr<DrawingBuffer> drawing_buffer,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    std::unique_ptr<gpu::SharedImageTexture> shared_image_texture)
    : owning_thread_ref(base::PlatformThread::CurrentRef()),
      drawing_buffer(std::move(drawing_buffer)),
      shared_image(std::move(shared_image)),
      shared_image_texture_(std::move(shared_image_texture)) {
  CHECK(this->shared_image);
}

DrawingBuffer::ColorBuffer::~ColorBuffer() {
  if (scoped_shared_image_access_) {
    gpu::SharedImageTexture::ScopedAccess::EndAccess(
        std::move(scoped_shared_image_access_));
  }

  if (base::PlatformThread::CurrentRef() != owning_thread_ref ||
      !drawing_buffer) {
    // If the context has been destroyed no cleanup is necessary since all
    // resources below are automatically destroyed. Note that if a ColorBuffer
    // is being destroyed on a different thread, it implies that the owning
    // thread was destroyed which means the associated context was also
    // destroyed.
    CHECK(!shared_image_texture_);
    return;
  }

  gpu::gles2::GLES2Interface* gl = drawing_buffer->gl_;
  if (!gl) {
    // Guard against in-flight destruction of the DrawingBuffer, while
    // still performing cleanup during BeginDestruction().
    return;
  }
  WebGraphicsContext3DProvider* provider = drawing_buffer->ContextProvider();
  if (!provider) {
    // Guard against in-flight destruction of the DrawingBuffer, while
    // still performing cleanup during BeginDestruction().
    return;
  }
  gpu::SharedImageInterface* sii = provider->SharedImageInterface();
  if (!sii) {
    // Guard against in-flight destruction of the DrawingBuffer, while
    // still performing cleanup during BeginDestruction().
    return;
  }

  shared_image->UpdateDestructionSyncToken(receive_sync_token);
  shared_image_texture_.reset();
}

void DrawingBuffer::ColorBuffer::BeginAccess(const gpu::SyncToken& sync_token,
                                             bool readonly) {
  scoped_shared_image_access_ =
      shared_image_texture_->BeginAccess(sync_token, readonly);
}

base::ByteSize DrawingBuffer::ColorBuffer::EstimatedSizeInBytes() const {
  return shared_image->EstimatedSizeInBytes();
}

gpu::SyncToken DrawingBuffer::ColorBuffer::EndAccess() {
  return gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(scoped_shared_image_access_));
}

void DrawingBuffer::ColorBuffer::ForceCleanUp() {
  if (scoped_shared_image_access_) {
    EndAccess();
  }
  shared_image_texture_.reset();
}

bool DrawingBuffer::Initialize(const gfx::Size& size, bool use_multisampling) {
  ScopedStateRestorer scoped_state_restorer(this);

  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // Need to try to restore the context again later.
    DLOG(ERROR) << "Cannot initialize with lost context.";
    return false;
  }

  gl_->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);

  int max_sample_count = 0;
  if (use_multisampling) {
    gl_->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &max_sample_count);
  }

  auto webgl_preferences = ContextProvider()->GetWebglPreferences();

  // We can't use anything other than explicit resolve for swap chain, as the
  // D3D11 texture backing the back buffer is single-sampled.
  bool supports_implicit_resolve =
      !using_swap_chain_ && extensions_util_->SupportsExtension(
                                "GL_EXT_multisampled_render_to_texture");

  const auto& gpu_feature_info = ContextProvider()->GetGpuFeatureInfo();
  // With graphite, Skia is not using ANGLE, so ANGLE will never be able to know
  // when the back buffer is sampled by Skia, so we can't use implicit resolve.
  supports_implicit_resolve =
      supports_implicit_resolve &&
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE] !=
          gpu::kGpuFeatureStatusEnabled;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
  // crbug.com/376174085: On Mac using implicit resolve causes flickering due to
  // losses of precision when render passes are interleaved. So disabling it.
  supports_implicit_resolve = false;
#endif

  if (webgl_preferences.anti_aliasing_mode == kAntialiasingModeUnspecified) {
    if (use_multisampling) {
      anti_aliasing_mode_ = kAntialiasingModeMSAAExplicitResolve;
      if (supports_implicit_resolve) {
        anti_aliasing_mode_ = kAntialiasingModeMSAAImplicitResolve;
      }
    } else {
      anti_aliasing_mode_ = kAntialiasingModeNone;
    }
  } else {
    bool prefer_implicit_resolve = (webgl_preferences.anti_aliasing_mode ==
                                    kAntialiasingModeMSAAImplicitResolve);
    if (prefer_implicit_resolve && !supports_implicit_resolve) {
      DLOG(ERROR) << "Invalid anti-aliasing mode specified.";
      return false;
    }
    anti_aliasing_mode_ = webgl_preferences.anti_aliasing_mode;
  }

  sample_count_ = std::min(
      static_cast<int>(webgl_preferences.msaa_sample_count), max_sample_count);
  eqaa_storage_sample_count_ = webgl_preferences.eqaa_storage_sample_count;
  if (ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
          gpu::USE_EQAA_STORAGE_SAMPLES_2))
    eqaa_storage_sample_count_ = 2;
  if (extensions_util_->SupportsExtension(
          "GL_AMD_framebuffer_multisample_advanced"))
    has_eqaa_support = true;

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

  if (!ResizeFramebufferInternal(requested_format_, requested_alpha_type_,
                                 size)) {
    DLOG(ERROR) << "Initialization failed to allocate backbuffer of size "
                << size.width() << " x " << size.height() << ".";
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

void DrawingBuffer::CopyStagingTextureToBackColorBufferIfNeeded() {
  if (!staging_texture_) {
    return;
  }

  // The rendering results are in `staging_texture_` rather than the
  // `back_color_buffer_`'s texture. Copy them over, doing any conversion
  // from the requested format to the SharedImage-supported format.
  const GLboolean do_flip_y = GL_FALSE;
  const GLboolean do_premultiply_alpha =
      back_color_buffer_->shared_image->alpha_type() == kPremul_SkAlphaType &&
      requested_alpha_type_ == kUnpremul_SkAlphaType;
  const GLboolean do_unpremultiply_alpha = GL_FALSE;
  gl_->CopySubTextureCHROMIUM(
      staging_texture_, 0, back_color_buffer_->shared_image->GetTextureTarget(),
      back_color_buffer_->texture_id(), 0, 0, 0, 0, 0, size_.width(),
      size_.height(), do_flip_y, do_premultiply_alpha, do_unpremultiply_alpha);
}

std::optional<gpu::SyncToken> DrawingBuffer::CopyToPlatformInternal(
    gpu::InterfaceBase* dst_interface,
    bool dst_is_unpremul_gl,
    SourceDrawingBuffer src_buffer,
    CopyFunctionRef copy_function) {
  ScopedStateRestorer scoped_state_restorer(this);

  gpu::gles2::GLES2Interface* src_gl = gl_;

  if (contents_changed_) {
    ResolveIfNeeded(kDontDiscard);
    src_gl->Flush();
  }

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  gpu::SyncToken produce_sync_token;
  bool need_restore_access = false;
  scoped_refptr<ColorBuffer> src_color_buffer;
  SkAlphaType src_alpha_type = kUnknown_SkAlphaType;
  if (src_buffer == kFrontBuffer && front_color_buffer_) {
    src_color_buffer = front_color_buffer_;
    src_alpha_type = src_color_buffer->shared_image->alpha_type();
    produce_sync_token = src_color_buffer->produce_sync_token;
  } else {
    src_color_buffer = back_color_buffer_;
    src_alpha_type = src_color_buffer->shared_image->alpha_type();
    need_restore_access = true;
    if (staging_texture_) {
      // The source for the copy must be a SharedImage that is accessible to
      // `dst_interface`. If the rendering results are in `staging_texture_`,
      // then they cannot be accessed by `dst_interface`. Copy the results
      // to `back_color_buffer`, without any (e.g alpha premultiplication)
      // conversion.
      if (dst_is_unpremul_gl) {
        // In this situation we are copying to another WebGL context that has
        // unpremultiplied alpha, and it is required that we do not lose the
        // precision that premultiplication would cause.
        const GLboolean do_flip_y = GL_FALSE;
        const GLboolean do_premultiply_alpha = GL_FALSE;
        const GLboolean do_unpremultiply_alpha = GL_FALSE;
        gl_->CopySubTextureCHROMIUM(
            staging_texture_, 0,
            back_color_buffer_->shared_image->GetTextureTarget(),
            back_color_buffer_->texture_id(), 0, 0, 0, 0, 0, size_.width(),
            size_.height(), do_flip_y, do_premultiply_alpha,
            do_unpremultiply_alpha);
        src_alpha_type = requested_alpha_type_;
      } else {
        CopyStagingTextureToBackColorBufferIfNeeded();
      }
    }
    produce_sync_token = back_color_buffer_->EndAccess();
  }

  if (!produce_sync_token.HasData()) {
    // This should only happen if the context has been lost.
    return std::nullopt;
  }

  std::optional<gpu::SyncToken> sync_token = copy_function(
      src_color_buffer->shared_image, produce_sync_token, src_alpha_type);

  if (need_restore_access) {
    src_color_buffer->BeginAccess(sync_token.value_or(gpu::SyncToken()),
                                  /*readonly=*/false);
  }
  return sync_token;
}

bool DrawingBuffer::CopyToPlatformTexture(gpu::gles2::GLES2Interface* dst_gl,
                                          GLenum dst_texture_target,
                                          GLuint dst_texture,
                                          GLint dst_level,
                                          SkAlphaType dst_alpha_type,
                                          GrSurfaceOrigin dst_origin,
                                          const gfx::Point& dst_texture_offset,
                                          const gfx::Rect& src_rect,
                                          SourceDrawingBuffer src_buffer) {
  if (!Extensions3DUtil::CanUseCopyTextureCHROMIUM(dst_texture_target))
    return false;

  auto copy_function =
      [&](scoped_refptr<gpu::ClientSharedImage> src_shared_image,
          const gpu::SyncToken& produce_sync_token,
          SkAlphaType src_alpha_type) -> std::optional<gpu::SyncToken> {
    // If origin doesn't match, we need to flip.
    bool do_flip_y = src_shared_image->surface_origin() != dst_origin;

    // `src_rect` here is always in top-left coordinate space, but
    // CopySubTextureCHROMIUM source rect is in texture coordinate space, so we
    // need to adjust.
    auto src_sub_rectangle = src_rect;
    if (src_shared_image->surface_origin() == kBottomLeft_GrSurfaceOrigin) {
      src_sub_rectangle.set_y(size_.height() - src_sub_rectangle.bottom());
    }

    GLboolean unpack_premultiply_alpha_needed = GL_FALSE;
    GLboolean unpack_unpremultiply_alpha_needed = GL_FALSE;
    if (src_alpha_type == kPremul_SkAlphaType &&
        dst_alpha_type == kUnpremul_SkAlphaType) {
      unpack_unpremultiply_alpha_needed = GL_TRUE;
    } else if (src_alpha_type == kUnpremul_SkAlphaType &&
               dst_alpha_type == kPremul_SkAlphaType) {
      unpack_premultiply_alpha_needed = GL_TRUE;
    }

    auto src_si_texture = src_shared_image->CreateGLTexture(dst_gl);
    auto src_si_access =
        src_si_texture->BeginAccess(produce_sync_token, /*readonly=*/true);
    dst_gl->CopySubTextureCHROMIUM(
        src_si_access->texture_id(), 0, dst_texture_target, dst_texture,
        dst_level, dst_texture_offset.x(), dst_texture_offset.y(),
        src_sub_rectangle.x(), src_sub_rectangle.y(), src_sub_rectangle.width(),
        src_sub_rectangle.height(), do_flip_y, unpack_premultiply_alpha_needed,
        unpack_unpremultiply_alpha_needed);
    auto sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
        std::move(src_si_access));
    src_si_texture.reset();
    return sync_token;
  };
  return CopyToPlatformInternal(dst_gl, dst_alpha_type == kUnpremul_SkAlphaType,
                                src_buffer, copy_function)
      .has_value();
}

std::optional<gpu::SyncToken> DrawingBuffer::CopyToPlatformSharedImage(
    gpu::raster::RasterInterface* dst_raster_interface,
    const scoped_refptr<gpu::ClientSharedImage>& dst_shared_image,
    const gpu::SyncToken& dst_sync_token,
    const gfx::Point& dst_texture_offset,
    const gfx::Rect& src_sub_rectangle,
    SourceDrawingBuffer src_buffer) {
  auto copy_function =
      [&](scoped_refptr<gpu::ClientSharedImage> src_shared_image,
          const gpu::SyncToken& produce_sync_token,
          SkAlphaType src_alpha_type) -> std::optional<gpu::SyncToken> {
    std::unique_ptr<gpu::RasterScopedAccess> dst_access =
        dst_shared_image->BeginRasterAccess(dst_raster_interface,
                                            dst_sync_token,
                                            /*readonly=*/false);
    std::unique_ptr<gpu::RasterScopedAccess> src_access =
        src_shared_image->BeginRasterAccess(dst_raster_interface,
                                            produce_sync_token,
                                            /*readonly=*/true);

    dst_raster_interface->CopySharedImage(
        src_shared_image->mailbox(), dst_shared_image->mailbox(),
        dst_texture_offset.x(), dst_texture_offset.y(), src_sub_rectangle.x(),
        src_sub_rectangle.y(), src_sub_rectangle.width(),
        src_sub_rectangle.height());

    gpu::SyncToken sync_token =
        gpu::RasterScopedAccess::EndAccess(std::move(src_access));
    sync_token = gpu::RasterScopedAccess::EndAccess(std::move(dst_access));
    return sync_token;
  };

  return CopyToPlatformInternal(dst_raster_interface,
                                /*dst_is_unpremul_gl=*/false, src_buffer,
                                copy_function);
}

bool DrawingBuffer::CopyToVideoFrame(
    WebGraphicsContext3DVideoFramePool* frame_pool,
    SourceDrawingBuffer src_buffer,
    const gfx::ColorSpace& dst_color_space,
    WebGraphicsContext3DVideoFramePool::FrameReadyCallback callback) {
  // Ensure that `frame_pool` has not experienced a context loss.
  // https://crbug.com/1269230
  auto* raster_interface = frame_pool->GetRasterInterface();
  if (!raster_interface)
    return false;
  auto copy_function =
      [&](scoped_refptr<gpu::ClientSharedImage> src_shared_image,
          const gpu::SyncToken& produce_sync_token,
          SkAlphaType src_alpha_type) -> std::optional<gpu::SyncToken> {
    return frame_pool->CopyRGBATextureToVideoFrame(
        src_shared_image->size(), src_shared_image, produce_sync_token,
        dst_color_space, std::move(callback));
  };
  return CopyToPlatformInternal(raster_interface, /*dst_is_unpremul_gl=*/false,
                                src_buffer, copy_function)
      .has_value();
}

base::ByteSize DrawingBuffer::EstimatedSizeInBytes() const {
  base::ByteSize result;
  if (back_color_buffer_) {
    result += back_color_buffer_->EstimatedSizeInBytes();
  }
  for (const auto& buffer : recycled_color_buffer_queue_) {
    result += buffer->EstimatedSizeInBytes();
  }
  for (const auto& buffer : exported_color_buffers_) {
    result += buffer->EstimatedSizeInBytes();
  }
  if (staging_texture_needed_ || SampleCount() > 0) {
    result += base::ByteSize(color_buffer_format_.EstimatedSizeInBytes(size_)) *
              (SampleCount() + staging_texture_needed_);
  }
  if (HasDepthBuffer() || HasStencilBuffer()) {
    result += std::max(SampleCount(), 1) *
              base::ByteSize(base::checked_cast<uint64_t>(4 * size_.width() *
                                                          size_.height()));
  }
  return result;
}

cc::Layer* DrawingBuffer::CcLayer() {
  if (!layer_) {
    layer_ = cc::TextureLayer::Create(this);
    if (client_) {
      client_->DrawingBufferClientInitializeLayer(layer_.get());
    }

    layer_->SetIsDrawable(true);
    layer_->SetHitTestable(true);
    layer_->SetContentsOpaque(requested_alpha_type_ == kOpaque_SkAlphaType);
    layer_->SetBlendBackgroundColor(requested_alpha_type_ !=
                                    kOpaque_SkAlphaType);
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

  if (staging_texture_) {
    gl_->DeleteTextures(1, &staging_texture_);
    staging_texture_ = 0;
  }

  size_ = gfx::Size();

  back_color_buffer_ = nullptr;
  front_color_buffer_ = nullptr;
  multisample_renderbuffer_ = 0;
  depth_stencil_buffer_ = 0;
  multisample_fbo_ = 0;
  fbo_ = 0;

  client_ = nullptr;
}

bool DrawingBuffer::ReallocateDefaultFramebuffer(const gfx::Size& size,
                                                 bool only_reallocate_color) {
  DCHECK(state_restorer_);
  // Recreate back_color_buffer_.
  back_color_buffer_ = CreateColorBuffer(size);

  if (staging_texture_) {
    state_restorer_->SetTextureBindingDirty();
    gl_->DeleteTextures(1, &staging_texture_);
    staging_texture_ = 0;
  }
  if (staging_texture_needed_) {
    state_restorer_->SetTextureBindingDirty();
    gl_->GenTextures(1, &staging_texture_);
    gl_->BindTexture(GL_TEXTURE_2D, staging_texture_);
    GLenum internal_format = requested_format_;

    // TexStorage is not core in GLES2 (webgl1) and enabling (or emulating) it
    // universally can cause issues with BGRA formats.
    // See: crbug.com/1443160#c38
    bool use_tex_image = !texture_storage_enabled_;
    if (webgl_version_ == Platform::kWebGL1ContextType &&
        requested_format_ == GL_SRGB8_ALPHA8) {
      // On GLES2:
      //   * SRGB_ALPHA_EXT is not a valid internal format for TexStorage2DEXT.
      //   * SRGB8_ALPHA8 is not a renderable texture internal format.
      // Just use TexImage2D instead of TexStorage2DEXT.
      use_tex_image = true;
    }
    if (use_tex_image) {
      switch (requested_format_) {
        case GL_RGB8:
          internal_format = color_buffer_format_.HasAlpha() ? GL_RGBA : GL_RGB;
          break;
        case GL_SRGB8_ALPHA8:
          internal_format = GL_SRGB_ALPHA_EXT;
          break;
        case GL_RGBA8:
        case GL_RGBA16F:
          internal_format = GL_RGBA;
          break;
        default:
          NOTREACHED();
      }

      gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format, size.width(),
                      size.height(), 0, internal_format,
                      requested_format_ == GL_RGBA16F ? GL_HALF_FLOAT_OES
                                                      : GL_UNSIGNED_BYTE,
                      nullptr);
    } else {
      if (requested_format_ == GL_RGB8) {
        internal_format = color_buffer_format_.HasAlpha() ? GL_RGBA8 : GL_RGB8;
      }
      gl_->TexStorage2DEXT(GL_TEXTURE_2D, 1, internal_format, size.width(),
                           size.height());
    }
  }

  AttachColorBufferToReadFramebuffer();

  if (WantExplicitResolve()) {
    if (!ReallocateMultisampleRenderbuffer(size)) {
      return false;
    }
  }

  if (WantDepthOrStencil() && !only_reallocate_color) {
    state_restorer_->SetFramebufferBindingDirty();
    state_restorer_->SetRenderbufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER,
                         multisample_fbo_ ? multisample_fbo_ : fbo_);
    if (!depth_stencil_buffer_)
      gl_->GenRenderbuffers(1, &depth_stencil_buffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer_);
    if (anti_aliasing_mode_ == kAntialiasingModeMSAAImplicitResolve) {
      gl_->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sample_count_,
                                             GL_DEPTH24_STENCIL8_OES,
                                             size.width(), size.height());
    } else if (anti_aliasing_mode_ == kAntialiasingModeMSAAExplicitResolve) {
      gl_->RenderbufferStorageMultisampleCHROMIUM(
          GL_RENDERBUFFER, sample_count_, GL_DEPTH24_STENCIL8_OES, size.width(),
          size.height());
    } else {
      gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                               size.width(), size.height());
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
  ClearFramebuffersInternal(clear_mask, kClearAllFBOs);
}

void DrawingBuffer::ClearFramebuffersInternal(GLbitfield clear_mask,
                                              ClearOption clear_option) {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();

  GLenum prev_draw_buffer =
      draw_buffer_ == GL_BACK ? GL_COLOR_ATTACHMENT0 : draw_buffer_;

  // Clear the multisample FBO, but also clear the non-multisampled buffer if
  // requested.
  if (multisample_fbo_ && clear_option == kClearAllFBOs) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    ScopedDrawBuffer scoped_draw_buffer(gl_, prev_draw_buffer,
                                        GL_COLOR_ATTACHMENT0);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
  }

  if (multisample_fbo_ || clear_option == kClearAllFBOs) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER,
                         multisample_fbo_ ? multisample_fbo_ : fbo_);
    ScopedDrawBuffer scoped_draw_buffer(gl_, prev_draw_buffer,
                                        GL_COLOR_ATTACHMENT0);
    gl_->Clear(clear_mask);
  }
}

void DrawingBuffer::ClearNewlyAllocatedFramebuffers(ClearOption clear_option) {
  DCHECK(state_restorer_);

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

  ClearFramebuffersInternal(clear_mask, clear_option);
}

gfx::Size DrawingBuffer::AdjustSize(const gfx::Size& desired_size,
                                    const gfx::Size& cur_size,
                                    int max_texture_size) {
  gfx::Size adjusted_size = desired_size;

  // Clamp if the desired size is greater than the maximum texture size for the
  // device.
  if (adjusted_size.height() > max_texture_size)
    adjusted_size.set_height(max_texture_size);

  if (adjusted_size.width() > max_texture_size)
    adjusted_size.set_width(max_texture_size);

  return adjusted_size;
}

bool DrawingBuffer::Resize(const gfx::Size& new_size) {
  ScopedStateRestorer scoped_state_restorer(this);
  return ResizeFramebufferInternal(requested_format_, requested_alpha_type_,
                                   new_size);
}

bool DrawingBuffer::ResizeWithFormat(GLenum requested_format,
                                     SkAlphaType requested_alpha_type,
                                     const gfx::Size& new_size) {
  ScopedStateRestorer scoped_state_restorer(this);
  return ResizeFramebufferInternal(requested_format, requested_alpha_type,
                                   new_size);
}

bool DrawingBuffer::ResizeFramebufferInternal(GLenum requested_format,
                                              SkAlphaType requested_alpha_type,
                                              const gfx::Size& new_size) {
  DCHECK(state_restorer_);
  DCHECK(!new_size.IsEmpty());
  bool needs_reallocate = false;

  gfx::Size adjusted_size = AdjustSize(new_size, size_, max_texture_size_);
  if (adjusted_size.IsEmpty()) {
    return false;
  }
  needs_reallocate |= adjusted_size != size_;

  // Initialize the alpha allocation settings based on the features and
  // workarounds in use.
  needs_reallocate |= requested_format_ != requested_format;
  requested_format_ = requested_format;
  switch (requested_format_) {
    case GL_RGB8:
      color_buffer_format_ = viz::SinglePlaneFormat::kRGBX_8888;
      // The following workarounds are used in order of importance; the
      // first is a correctness issue, the second a major performance
      // issue, and the third a minor performance issue.
      if (ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
              gpu::DISABLE_GL_RGB_FORMAT)) {
        // This configuration will
        //  - allow invalid CopyTexImage to RGBA targets
        //  - fail valid FramebufferBlit from RGB targets
        // https://crbug.com/776269
        color_buffer_format_ = viz::SinglePlaneFormat::kRGBA_8888;
      } else if (WantExplicitResolve() &&
                 ContextProvider()->GetGpuFeatureInfo().IsWorkaroundEnabled(
                     gpu::DISABLE_WEBGL_RGB_MULTISAMPLING_USAGE)) {
        // This configuration avoids the above issues because
        //  - CopyTexImage is invalid from multisample renderbuffers
        //  - FramebufferBlit is invalid to multisample renderbuffers
        color_buffer_format_ = viz::SinglePlaneFormat::kRGBA_8888;
      }
      break;
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
      color_buffer_format_ = viz::SinglePlaneFormat::kRGBA_8888;
      break;
    case GL_RGBA16F:
      color_buffer_format_ = viz::SinglePlaneFormat::kRGBA_F16;
      break;
    default:
      NOTREACHED();
  }
  needs_reallocate |= requested_alpha_type_ != requested_alpha_type;
  requested_alpha_type_ = requested_alpha_type;

  if (needs_reallocate) {
    do {
      if (!ReallocateDefaultFramebuffer(adjusted_size,
                                        /*only_reallocate_color=*/false)) {
        adjusted_size =
            gfx::ScaleToFlooredSize(adjusted_size, kResourceAdjustedRatio);
        continue;
      }
      break;
    } while (!adjusted_size.IsEmpty());

    size_ = adjusted_size;
    // Free all mailboxes, because they are now of the wrong size. Only the
    // first call in this loop has any effect.
    recycled_color_buffer_queue_.clear();
    recycled_software_resources_.clear();

    if (adjusted_size.IsEmpty())
      return false;
  }

  ClearNewlyAllocatedFramebuffers(kClearAllFBOs);
  return true;
}

void DrawingBuffer::SetColorSpace(PredefinedColorSpace predefined_color_space) {
  // Color space changes that are no-ops should not reach this point.
  const gfx::ColorSpace color_space =
      PredefinedColorSpaceToGfxColorSpace(predefined_color_space);
  DCHECK_NE(color_space, color_space_);
  color_space_ = color_space;

  ScopedStateRestorer scoped_state_restorer(this);

  // Free all mailboxes, because they are now of the wrong color space.
  recycled_color_buffer_queue_.clear();
  recycled_software_resources_.clear();

  if (!ReallocateDefaultFramebuffer(size_, /*only_reallocate_color=*/true)) {
    // TODO(https://crbug.com/1208480): What is the correct behavior is we fail
    // to re-allocate the buffer.
    DLOG(ERROR) << "Failed to allocate color buffer with new color space.";
  }

  ClearNewlyAllocatedFramebuffers(kClearAllFBOs);
}

bool DrawingBuffer::ResolveAndBindForReadAndDraw() {
  {
    ScopedStateRestorer scoped_state_restorer(this);
    ResolveIfNeeded(kDontDiscard);
    // Note that in rare situations on macOS the drawing buffer can be
    // destroyed during the resolve process, specifically during
    // automatic graphics switching. Guard against this.
    if (destruction_in_progress_)
      return false;
  }
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  return true;
}

void DrawingBuffer::ResolveMultisampleFramebufferInternal() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  if (WantExplicitResolve()) {
    state_restorer_->SetClearStateDirty();
    gl_->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, multisample_fbo_);
    gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, fbo_);
    gl_->Disable(GL_SCISSOR_TEST);

    int width = size_.width();
    int height = size_.height();
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
}

void DrawingBuffer::ResolveIfNeeded(DiscardBehavior discardBehavior) {
  DCHECK(state_restorer_);
  if (anti_aliasing_mode_ != kAntialiasingModeNone) {
    if (preserve_drawing_buffer_ == kDiscard &&
        discard_framebuffer_supported_ && discardBehavior == kDiscardAllowed &&
        !transient_framebuffers_discarded_) {
      // Discard the depth and stencil buffers as early as possible, before
      // making any potentially-unneeded calls to BindFramebuffer (even no-ops),
      // in order to maximize the chances that their storage can be kept in tile
      // memory.
      const GLenum kAttachments[2] = {GL_DEPTH_ATTACHMENT,
                                      GL_STENCIL_ATTACHMENT};
      state_restorer_->SetFramebufferBindingDirty();
      gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
      gl_->DiscardFramebufferEXT(GL_FRAMEBUFFER, 2, kAttachments);
      transient_framebuffers_discarded_ = true;
    }
    if (!contents_change_resolved_) {
      ResolveMultisampleFramebufferInternal();
    }
  }
  contents_change_resolved_ = true;

  auto* gl = ContextProvider()->ContextGL();
  gl::GpuPreference active_gpu = gl::GpuPreference::kDefault;
  if (gl->DidGpuSwitch(&active_gpu) == GL_TRUE) {
    // This code path is mainly taken on macOS (the only platform which, as of
    // this writing, dispatches the GPU-switched notifications), and the
    // comments below focus only on macOS.
    //
    // The code below attempts to deduce whether, if a GPU switch occurred,
    // it's really necessary to lose the context because certain GPU resources
    // are no longer accessible. Resources only become inaccessible if
    // CGLSetVirtualScreen is explicitly called against a GL context to change
    // its renderer ID. GPU switching notifications are highly asynchronous.
    //
    // The tests below, of the initial and currently active GPU, replicate
    // some logic in GLContextCGL::ForceGpuSwitchIfNeeded. Basically, if a
    // context requests the high-performance GPU, then CGLSetVirtualScreen
    // will never be called to migrate that context to the low-power
    // GPU. However, contexts that were allocated on the integrated GPU will
    // be migrated to the discrete GPU, and back, when the discrete GPU is
    // activated and deactivated. Also, if the high-performance GPU was
    // requested, then that request took effect during context bringup, even
    // though the GPU switching notification is generally dispatched a couple
    // of seconds after that, so it's not necessary to either lose the context
    // or reallocate the multisampled renderbuffers when that initial
    // notification is received.
    if (initial_gpu_ == gl::GpuPreference::kLowPower &&
        current_active_gpu_ != active_gpu) {
      if ((WantExplicitResolve() && preserve_drawing_buffer_ == kPreserve) ||
          client_
              ->DrawingBufferClientUserAllocatedMultisampledRenderbuffers()) {
        // In these situations there are multisampled renderbuffers whose
        // content the application expects to be preserved, but which can not
        // be. Forcing a lost context is the only option to keep applications
        // rendering correctly.
        client_->DrawingBufferClientForceLostContextWithAutoRecovery(
            "Losing WebGL context because multisampled renderbuffers were "
            "allocated, to work around macOS OpenGL driver bugs");
      } else if (WantExplicitResolve()) {
        ReallocateMultisampleRenderbuffer(size_);

        // This does a bit more work than desired - clearing any depth and
        // stencil renderbuffers is unnecessary, since they weren't reallocated
        // - but reusing this code reduces complexity. Note that we do not clear
        // the non-multisampled framebuffer, as doing so can cause users'
        // content to disappear unexpectedly.
        //
        // TODO(crbug.com/1046146): perform this clear at the beginning rather
        // than at the end of a frame in order to eliminate rendering glitches.
        // This should also simplify the code, allowing removal of the
        // ClearOption.
        ClearNewlyAllocatedFramebuffers(kClearOnlyMultisampledFBO);
      }
    }
    current_active_gpu_ = active_gpu;
  }
}

bool DrawingBuffer::ReallocateMultisampleRenderbuffer(const gfx::Size& size) {
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetRenderbufferBindingDirty();
  gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
  gl_->BindRenderbuffer(GL_RENDERBUFFER, multisample_renderbuffer_);

  // Note that the multisample rendertarget will allocate an alpha channel
  // based on the ColorBuffer's format, since it will resolve into the
  // ColorBuffer.
  GLenum internal_format = requested_format_;
  if (requested_format_ == GL_RGB8) {
    internal_format = color_buffer_format_.HasAlpha() ? GL_RGBA8 : GL_RGB8;
  }

  if (has_eqaa_support) {
    gl_->RenderbufferStorageMultisampleAdvancedAMD(
        GL_RENDERBUFFER, sample_count_, eqaa_storage_sample_count_,
        internal_format, size.width(), size.height());
  } else {
    gl_->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                                internal_format, size.width(),
                                                size.height());
  }

  if (gl_->GetError() == GL_OUT_OF_MEMORY)
    return false;

  gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_RENDERBUFFER, multisample_renderbuffer_);
  return true;
}

void DrawingBuffer::RestoreFramebufferBindings() {
  // Can be called with ScopedDrawingBufferBinder on the stack after
  // context loss. Null checking client_ is insufficient.
  if (destruction_in_progress_) {
    return;
  }
  client_->DrawingBufferClientRestoreFramebufferBinding();
}

void DrawingBuffer::RestoreAllState() {
  client_->DrawingBufferClientRestoreScissorTest();
  client_->DrawingBufferClientRestoreMaskAndClearValues();
  client_->DrawingBufferClientRestorePixelPackParameters();
  client_->DrawingBufferClientRestoreTexture2DBinding();
  client_->DrawingBufferClientRestoreTextureCubeMapBinding();
  client_->DrawingBufferClientRestoreRenderbufferBinding();
  client_->DrawingBufferClientRestoreFramebufferBinding();
  client_->DrawingBufferClientRestorePixelUnpackBufferBinding();
  client_->DrawingBufferClientRestorePixelPackBufferBinding();
}

bool DrawingBuffer::SupportsNoCopyExportForLowLatency() {
  if (!SharedGpuContext::IsGpuCompositingEnabled()) {
    // If SW compositing is being used, the shared GPU context has no raster
    // interface and hence no way to read back an accelerated SharedImage. In
    // that case, it is not viable to directly export the DrawingBuffer's
    // SharedImage to a use case that is external to WebGL; instead, the
    // internal caller of this method must read back the DrawingBuffer's SI via
    // the WebGL context and then pass that result back to their external
    // entrypoint (as e.g. an unaccelerated bitmap or software SI).
    return false;
  }

  if (!back_color_buffer_) {
    return false;
  }

  // If the back buffer has concurrent R/W usage, then it means that (a) we are
  // in low-latency mode, and (b) we determined that it is possible to support
  // concurrent read/writes on the back buffer's SI.
  return back_color_buffer_->shared_image->usage().Has(
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
}

bool DrawingBuffer::Multisample() const {
  return anti_aliasing_mode_ != kAntialiasingModeNone;
}

void DrawingBuffer::Bind(GLenum target) {
  gl_->BindFramebuffer(target, WantExplicitResolve() ? multisample_fbo_ : fbo_);
}

GLenum DrawingBuffer::StorageFormat() const {
  return requested_format_;
}

scoped_refptr<StaticBitmapImage>
DrawingBuffer::GetRGBAUnacceleratedStaticBitmapImage(
    SourceDrawingBuffer source_buffer) {
  // Readback in native GL byte order (RGBA).
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  if (RuntimeEnabledFeatures::WebGLDrawingBufferStorageEnabled() &&
      back_color_buffer_->shared_image->format() ==
          viz::SinglePlaneFormat::kRGBA_F16) {
    format = viz::SinglePlaneFormat::kRGBA_F16;
  }

  return GetUnacceleratedStaticBitmapImage(
      source_buffer, format, requested_alpha_type_, kTopLeft_GrSurfaceOrigin);
}

scoped_refptr<StaticBitmapImage>
DrawingBuffer::GetUnacceleratedStaticBitmapImage(
    SourceDrawingBuffer source_buffer,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin) {
  ScopedStateRestorer scoped_state_restorer(this);

  sk_sp<SkData> dst_buffer = TryAllocateSkDataForBitmap(format, Size());
  if (!dst_buffer)
    return nullptr;

  auto pixels = skia::as_writable_byte_span(*dst_buffer);
  ReadBackFramebuffer(pixels, format, alpha_type, origin, source_buffer);

  return StaticBitmapImage::Create(
      std::move(dst_buffer),
      SkImageInfo::Make(SkISize::Make(Size().width(), Size().height()),
                        ToClosestSkColorType(format), alpha_type,
                        color_space_.ToSkColorSpace()),
      origin == kTopLeft_GrSurfaceOrigin
          ? ImageOrientationEnum::kOriginTopLeft
          : ImageOrientationEnum::kOriginBottomLeft);
}

void DrawingBuffer::ReadBackFramebuffer(
    base::span<uint8_t> pixels,
    viz::SharedImageFormat destination_format,
    SkAlphaType destination_alpha_type,
    GrSurfaceOrigin destination_origin,
    SourceDrawingBuffer source_buffer) {
  DCHECK(state_restorer_);

  GLuint fbo = 0;

  state_restorer_->SetFramebufferBindingDirty();
  // Generate new fbo for front buffer if needed.
  if (source_buffer == kFrontBuffer && front_color_buffer_) {
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    front_color_buffer_->BeginAccess(gpu::SyncToken(), /*readonly=*/true);
    gl_->FramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        front_color_buffer_->shared_image->GetTextureTarget(),
        front_color_buffer_->texture_id(), 0);
  } else {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }

  state_restorer_->SetPixelPackParametersDirty();
  gl_->PixelStorei(GL_PACK_ALIGNMENT, 1);
  if (webgl_version_ != Platform::kWebGL1ContextType) {
    gl_->PixelStorei(GL_PACK_SKIP_ROWS, 0);
    gl_->PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    gl_->PixelStorei(GL_PACK_ROW_LENGTH, 0);

    state_restorer_->SetPixelPackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  }

  GLenum data_type = GL_UNSIGNED_BYTE;

  base::CheckedNumeric<size_t> row_bytes = 4;
  if (destination_format == viz::SinglePlaneFormat::kRGBA_F16) {
    data_type = (webgl_version_ != Platform::kWebGL1ContextType)
                    ? GL_HALF_FLOAT
                    : GL_HALF_FLOAT_OES;
    row_bytes *= 2;
  }
  row_bytes *= Size().width();

  base::CheckedNumeric<size_t> num_rows = Size().height();
  base::CheckedNumeric<size_t> expected_data_size = num_rows * row_bytes;

  DCHECK_EQ(expected_data_size.ValueOrDie(), pixels.size());

  gl_->ReadPixels(0, 0, Size().width(), Size().height(), GL_RGBA, data_type,
                  pixels.data());

  // For half float storage Skia order is RGBA, hence no swizzling is needed.
  if (destination_format == viz::SinglePlaneFormat::kBGRA_8888) {
    // Swizzle red and blue channels to match SkBitmap's byte ordering.
    // TODO(kbr): expose GL_BGRA as extension.
    for (size_t i = 0; i < pixels.size(); i += 4) {
      std::swap(pixels[i], pixels[i + 2]);
    }
  }

  WebGLImageConversion::AlphaOp op = WebGLImageConversion::kAlphaDoNothing;
  if (requested_alpha_type_ == kUnpremul_SkAlphaType &&
      destination_alpha_type == kPremul_SkAlphaType) {
    op = WebGLImageConversion::kAlphaDoPremultiply;
  } else if (requested_alpha_type_ != kUnpremul_SkAlphaType &&
             destination_alpha_type == kUnpremul_SkAlphaType) {
    // We don't support unpremultiplication.
    NOTREACHED();
  }

  if (op == WebGLImageConversion::kAlphaDoPremultiply) {
    for (size_t i = 0; i < pixels.size(); i += 4) {
      uint8_t alpha = pixels[i + 3];
      for (size_t j = 0; j < 3; j++)
        pixels[i + j] = (pixels[i + j] * alpha + 127) / 255;
    }
  } else if (op != WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
  }

  // ReadPixels always reads with bottom-left origin regardless of the
  // `opengl_flip_y_extension_`
  if (destination_origin != kBottomLeft_GrSurfaceOrigin) {
    FlipVertically(pixels, num_rows.ValueOrDie(), row_bytes.ValueOrDie());
  }

  if (fbo) {
    // The front buffer was used as the source of the pixels via |fbo|; clean up
    // |fbo| and release access to the front buffer's SharedImage now that the
    // readback is finished.
    gl_->FramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        front_color_buffer_->shared_image->GetTextureTarget(), 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
    front_color_buffer_->EndAccess();
  }
}

scoped_refptr<DrawingBuffer::ColorBuffer> DrawingBuffer::CreateColorBuffer(
    const gfx::Size& size) {
  if (size.IsEmpty()) {
    // Context is likely lost.
    return nullptr;
  }

  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  gpu::SharedImageInterface* sii = ContextProvider()->SharedImageInterface();

  scoped_refptr<gpu::ClientSharedImage> back_buffer_shared_image;

  // The SharedImages created here are read to and written from by WebGL. They
  // may also be read via the raster interface for WebGL->video and/or
  // WebGL->canvas conversions.
  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                   gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
                                   gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_RASTER_READ;
  if (initial_gpu_ == gl::GpuPreference::kHighPerformance)
    usage |= gpu::SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU;
  GrSurfaceOrigin origin = opengl_flip_y_extension_
                               ? kTopLeft_GrSurfaceOrigin
                               : kBottomLeft_GrSurfaceOrigin;

#if BUILDFLAG(IS_MAC)
  // For Mac, explicitly specify BGRA/X instead of RGBA/X so that IOSurface
  // format matches shared image format. This is necessary for Graphite where
  // IOSurfaces are always used to allow sharing between ANGLE and Dawn.
  if (color_buffer_format_ == viz::SinglePlaneFormat::kRGBA_8888 &&
      ContextProvider()->GetCapabilities().gpu_memory_buffer_formats.Has(
          viz::SinglePlaneSharedImageFormatToBufferFormat(
              viz::SinglePlaneFormat::kBGRA_8888))) {
    color_buffer_format_ = viz::SinglePlaneFormat::kBGRA_8888;
  } else if (color_buffer_format_ == viz::SinglePlaneFormat::kRGBX_8888 &&
             ContextProvider()->GetCapabilities().gpu_memory_buffer_formats.Has(
                 viz::SinglePlaneSharedImageFormatToBufferFormat(
                     viz::SinglePlaneFormat::kBGRX_8888))) {
    color_buffer_format_ = viz::SinglePlaneFormat::kBGRX_8888;
  }
#endif  // BUILDFLAG(IS_MAC)

  SkAlphaType back_buffer_alpha_type = kPremul_SkAlphaType;
  if (using_swap_chain_) {
    usage = usage | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    usage = usage | gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
  } else {
    // First see if creating a SharedImage that can be used as an overlay is
    // feasible.
    if (ShouldUseChromiumImage()) {
#if !BUILDFLAG(IS_ANDROID)
      // Android's SharedImage backing for ChromiumImage does not support BGRX.

      // TODO(b/286417069): BGRX has issues when Vulkan is used for raster and
      // composite. Using BGRX is technically possible but will require a lot
      // of work given the current state of the codebase. There are projects in
      // flight that will make using BGRX a lot easier, but until then, simply
      // use RGBX when Vulkan is enabled.
      const auto& gpu_feature_info = ContextProvider()->GetGpuFeatureInfo();
      const bool allow_bgrx =
          gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] !=
          gpu::kGpuFeatureStatusEnabled;

      // For ChromeOS explicitly specify BGRX instead of RGBX since some older
      // Intel GPUs (i8xx) don't support RGBX overlays.
      if (color_buffer_format_ == viz::SinglePlaneFormat::kRGBX_8888 &&
          allow_bgrx &&
          ContextProvider()->GetCapabilities().gpu_memory_buffer_formats.Has(
              viz::SinglePlaneSharedImageFormatToBufferFormat(
                  viz::SinglePlaneFormat::kBGRX_8888))) {
        color_buffer_format_ = viz::SinglePlaneFormat::kBGRX_8888;
      }
#endif  // !BUILDFLAG(IS_ANDROID)

      if (ContextProvider()->GetCapabilities().gpu_memory_buffer_formats.Has(
              viz::SinglePlaneSharedImageFormatToBufferFormat(
                  color_buffer_format_))) {
        usage = usage | gpu::SHARED_IMAGE_USAGE_SCANOUT;
        if (low_latency_enabled()) {
          usage = usage | gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
        }
      }
    }

    // Set the correct SkAlphaType on the new shared image if not using as an
    // overlay (note that in the case of creating a SharedImage that can be
    // used as an overlay we instead keep this buffer premultiplied, draw to
    // |premultiplied_alpha_false_mailbox_|, and convert during copy).
    if (requested_alpha_type_ == kUnpremul_SkAlphaType &&
        !usage.Has(gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
      back_buffer_alpha_type = kUnpremul_SkAlphaType;
    }
  }

  back_buffer_shared_image = sii->CreateSharedImage(
      {color_buffer_format_, size, color_space_, origin, back_buffer_alpha_type,
       usage, "WebGLDrawingBuffer"},
      gpu::kNullSurfaceHandle);

  staging_texture_needed_ = false;
  if (requested_alpha_type_ == kUnpremul_SkAlphaType &&
      requested_alpha_type_ != back_buffer_alpha_type) {
    // If it was requested that our format be unpremultiplied, but the
    // backbuffer that we will use for compositing will be premultiplied (e.g,
    // because it be used as an overlay), then we will need to create a separate
    // unpremultiplied staging backbuffer for WebGL to render to.
    staging_texture_needed_ = true;
  }
  if (requested_format_ == GL_SRGB8_ALPHA8) {
    // SharedImages do not support sRGB texture formats, so a staging texture is
    // always needed for them.
    staging_texture_needed_ = true;
  }

  // Import the backbuffer of swap chain or allocated SharedImage into GL.
  std::unique_ptr<gpu::SharedImageTexture> si_texture =
      back_buffer_shared_image->CreateGLTexture(gl_);
  GLenum si_texture_target = back_buffer_shared_image->GetTextureTarget();
  scoped_refptr<DrawingBuffer::ColorBuffer> color_buffer =
      base::MakeRefCounted<ColorBuffer>(weak_factory_.GetWeakPtr(),
                                        std::move(back_buffer_shared_image),
                                        std::move(si_texture));
  color_buffer->BeginAccess(gpu::SyncToken(), /*readonly=*/false);
  gl_->BindTexture(si_texture_target, color_buffer->texture_id());

  // Clear the alpha channel if RGB emulation is required.
  if (DefaultBufferRequiresAlphaChannelToBePreserved()) {
    GLuint fbo = 0;

    state_restorer_->SetClearStateDirty();
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              si_texture_target, color_buffer->texture_id(), 0);
    gl_->ClearColor(0, 0, 0, 1);
    gl_->ColorMask(false, false, false, true);
    gl_->Disable(GL_SCISSOR_TEST);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              si_texture_target, 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
  }

  return color_buffer;
}

void DrawingBuffer::AttachColorBufferToReadFramebuffer() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  GLenum id = 0;
  GLenum texture_target = 0;

  if (staging_texture_) {
    id = staging_texture_;
    texture_target = GL_TEXTURE_2D;
  } else {
    id = back_color_buffer_->texture_id();
    texture_target = back_color_buffer_->shared_image->GetTextureTarget();
  }

  gl_->BindTexture(texture_target, id);

  if (anti_aliasing_mode_ == kAntialiasingModeMSAAImplicitResolve) {
    gl_->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, id, 0,
        sample_count_);
  } else {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target, id, 0);
  }
}

bool DrawingBuffer::WantExplicitResolve() {
  return anti_aliasing_mode_ == kAntialiasingModeMSAAExplicitResolve;
}

bool DrawingBuffer::WantDepthOrStencil() {
  return want_depth_ || want_stencil_;
}

DrawingBuffer::ScopedStateRestorer::ScopedStateRestorer(
    DrawingBuffer* drawing_buffer)
    : drawing_buffer_(drawing_buffer) {
  // If this is a nested restorer, save the previous restorer.
  previous_state_restorer_ = drawing_buffer->state_restorer_;
  drawing_buffer_->state_restorer_ = this;

  Client* client = drawing_buffer_->client_;
  if (!client) {
    return;
  }
  client->DrawingBufferClientInterruptPixelLocalStorage();
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
  client->DrawingBufferClientRestorePixelLocalStorage();
}

bool DrawingBuffer::ShouldUseChromiumImage() {
  if (chromium_image_usage_ != kAllowChromiumImage) {
    return false;
  }
  if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
    return true;
  }
  return low_latency_enabled() &&
         base::FeatureList::IsEnabled(features::kLowLatencyWebGLImageChromium);
}

}  // namespace blink
