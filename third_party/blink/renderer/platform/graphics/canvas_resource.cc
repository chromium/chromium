// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"

namespace blink {

// TODO(danakj): One day the gpu::mojom::Mailbox type should be shared with
// blink directly and we won't need to use gpu::mojom::blink::Mailbox, nor the
// conversion through WTF::Vector.
gpu::mojom::blink::MailboxPtr SharedBitmapIdToGpuMailboxPtr(
    const viz::SharedBitmapId& id) {
  WTF::Vector<int8_t> name(GL_MAILBOX_SIZE_CHROMIUM);
  for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
    name[i] = id.name[i];
  return {base::in_place, name};
}

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               SkFilterQuality filter_quality,
                               const CanvasColorParams& color_params)
    : provider_(std::move(provider)),
      filter_quality_(filter_quality),
      color_params_(color_params) {
  thread_of_origin_ = Platform::Current()->CurrentThread()->ThreadId();
}

CanvasResource::~CanvasResource() {
#if DCHECK_IS_ON()
  DCHECK(did_call_on_destroy_);
#endif
}

void CanvasResource::OnDestroy() {
  if (thread_of_origin_ != Platform::Current()->CurrentThread()->ThreadId()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists.
    Abandon();
  } else {
    TearDown();
  }
#if DCHECK_IS_ON()
  did_call_on_destroy_ = true;
#endif
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->ContextGL();
}

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    auto* gl = ContextGL();
    if (gl)
      gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
}

static void ReleaseFrameResources(
    base::WeakPtr<CanvasResourceProvider> resource_provider,
    scoped_refptr<CanvasResource> resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  resource->WaitSyncToken(sync_token);
  if (lost_resource) {
    resource->Abandon();
  }
  if (resource_provider && !lost_resource && resource->IsRecycleable()) {
    resource_provider->RecycleResource(std::move(resource));
  }
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback,
    MailboxSyncMode sync_mode) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  scoped_refptr<CanvasResource> this_ref(this);
  auto func = WTF::Bind(&ReleaseFrameResources, provider_,
                        WTF::Passed(std::move(this_ref)));
  *out_callback = viz::SingleReleaseCallback::Create(std::move(func));

  if (out_resource) {
    if (SupportsAcceleratedCompositing())
      return PrepareAcceleratedTransferableResource(out_resource, sync_mode);
    return PrepareUnacceleratedTransferableResource(out_resource);
  }
  return true;
}

bool CanvasResource::PrepareAcceleratedTransferableResource(
    viz::TransferableResource* out_resource,
    MailboxSyncMode sync_mode) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareAcceleratedTransferableResource");
  // Gpu compositing is a prerequisite for compositing an accelerated resource
  DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
  auto* gl = ContextGL();
  DCHECK(gl);
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(sync_mode);
  if (mailbox.IsZero())
    return false;

  *out_resource = viz::TransferableResource::MakeGLOverlay(
      mailbox, GLFilter(), TextureTarget(), GetSyncToken(), gfx::Size(Size()),
      IsOverlayCandidate());

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();
  out_resource->format = color_params_.TransferableResourceFormat();
  out_resource->read_lock_fences_enabled = NeedsReadLockFences();

  return true;
}

bool CanvasResource::PrepareUnacceleratedTransferableResource(
    viz::TransferableResource* out_resource) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareUnacceleratedTransferableResource");
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(kVerifiedSyncToken);
  if (mailbox.IsZero())
    return false;

  *out_resource = viz::TransferableResource::MakeSoftware(
      mailbox, gfx::Size(Size()), color_params_.TransferableResourceFormat());

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();

  return true;
}

GrContext* CanvasResource::GetGrContext() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->GetGrContext();
}

GLenum CanvasResource::GLFilter() const {
  return filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

// CanvasResourceBitmap
//==============================================================================

CanvasResourceBitmap::CanvasResourceBitmap(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      image_(std::move(image)) {}

CanvasResourceBitmap::~CanvasResourceBitmap() {
  OnDestroy();
}

scoped_refptr<CanvasResourceBitmap> CanvasResourceBitmap::Create(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params) {
  auto resource = AdoptRef(new CanvasResourceBitmap(
      std::move(image), std::move(provider), filter_quality, color_params));
  return resource->IsValid() ? resource : nullptr;
}

bool CanvasResourceBitmap::IsValid() const {
  return image_ ? image_->IsValid() : false;
}

bool CanvasResourceBitmap::IsAccelerated() const {
  return image_->IsTextureBacked();
}

scoped_refptr<CanvasResource> CanvasResourceBitmap::MakeAccelerated(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (IsAccelerated())
    return base::WrapRefCounted(this);

  TRACE_EVENT0("blink", "CanvasResourceBitmap::MakeAccelerated");

  if (!context_provider_wrapper)
    return nullptr;
  scoped_refptr<StaticBitmapImage> accelerated_image =
      image_->MakeAccelerated(context_provider_wrapper);
  // passing nullptr for the resource provider argument creates an orphan
  // CanvasResource, which implies that it internal resources will not be
  // recycled.
  scoped_refptr<CanvasResource> accelerated_resource =
      Create(accelerated_image, nullptr, FilterQuality(), ColorParams());
  if (!accelerated_resource)
    return nullptr;
  return accelerated_resource;
}

scoped_refptr<CanvasResource> CanvasResourceBitmap::MakeUnaccelerated() {
  if (!IsAccelerated())
    return base::WrapRefCounted(this);

  TRACE_EVENT0("blink", "CanvasResourceBitmap::MakeUnaccelerated");

  scoped_refptr<StaticBitmapImage> unaccelerated_image =
      image_->MakeUnaccelerated();
  // passing nullptr for the resource provider argument creates an orphan
  // CanvasResource, which implies that it internal resources will not be
  // recycled.
  scoped_refptr<CanvasResource> unaccelerated_resource =
      Create(unaccelerated_image, nullptr, FilterQuality(), ColorParams());
  return unaccelerated_resource;
}

void CanvasResourceBitmap::TearDown() {
  image_ = nullptr;
}

IntSize CanvasResourceBitmap::Size() const {
  if (!image_)
    return IntSize(0, 0);
  return IntSize(image_->width(), image_->height());
}

GLenum CanvasResourceBitmap::TextureTarget() const {
  return GL_TEXTURE_2D;
}

scoped_refptr<StaticBitmapImage> CanvasResourceBitmap::Bitmap() {
  return image_;
}

const gpu::Mailbox& CanvasResourceBitmap::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  image_->EnsureMailbox(sync_mode, GLFilter());
  return image_->GetMailbox();
}

bool CanvasResourceBitmap::HasGpuMailbox() const {
  return image_ && image_->HasMailbox();
}

const gpu::SyncToken CanvasResourceBitmap::GetSyncToken() {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  return image_->GetSyncToken();
}

void CanvasResourceBitmap::Transfer() {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  return image_->Transfer();
}

bool CanvasResourceBitmap::OriginClean() const {
  DCHECK(image_);
  return image_->OriginClean();
}

void CanvasResourceBitmap::SetOriginClean(bool value) {
  DCHECK(image_);
  image_->SetOriginClean(value);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceBitmap::ContextProviderWrapper() const {
  if (!image_)
    return nullptr;
  return image_->ContextProviderWrapper();
}

void CanvasResourceBitmap::TakeSkImage(sk_sp<SkImage> image) {
  DCHECK(IsAccelerated() == image->isTextureBacked());
  image_ =
      StaticBitmapImage::Create(std::move(image), ContextProviderWrapper());
}

// CanvasResourceGpuMemoryBuffer
//==============================================================================

CanvasResourceGpuMemoryBuffer::CanvasResourceGpuMemoryBuffer(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    bool is_accelerated)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_accelerated_(is_accelerated) {
  if (!context_provider_wrapper_)
    return;
  auto* gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  auto* gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (!gl || !gr)
    return;

  const gfx::BufferUsage buffer_usage =
      is_accelerated ? gfx::BufferUsage::SCANOUT
                     : gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!gpu_memory_buffer_manager)
    return;
  gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
      gfx::Size(size.Width(), size.Height()), ColorParams().GetBufferFormat(),
      buffer_usage, gpu::kNullSurfaceHandle);
  if (!gpu_memory_buffer_)
    return;

  gpu_memory_buffer_->SetColorSpace(color_params.GetStorageGfxColorSpace());

  image_id_ = gl->CreateImageCHROMIUM(gpu_memory_buffer_->AsClientBuffer(),
                                      size.Width(), size.Height(),
                                      ColorParams().GLUnsizedInternalFormat());
  if (!image_id_) {
    gpu_memory_buffer_ = nullptr;
    return;
  }
  gl->GenTextures(1, &texture_id_);
  const GLenum target = TextureTarget();
  gl->BindTexture(target, texture_id_);
  // TODO(mcasas): consider making |image_id_| a local variable and balancing
  // BindTexImage2DCHROMIUM() with DestroyImageCHROMIUM(), leaving |texture_id_|
  // to keep alive the Image2DCHROMIUM.
  gl->BindTexImage2DCHROMIUM(target, image_id_);

  if (is_accelerated_ && target == GL_TEXTURE_EXTERNAL_OES) {
    // We can't CopyTextureCHROMIUM() into a GL_TEXTURE_EXTERNAL_OES; create
    // another image and bind a GL_TEXTURE_2D texture to it.
    const GLuint image_2d_id_for_copy = gl->CreateImageCHROMIUM(
        gpu_memory_buffer_->AsClientBuffer(), size.Width(), size.Height(),
        ColorParams().GLUnsizedInternalFormat());

    gl->GenTextures(1, &texture_2d_id_for_copy_);
    gl->BindTexture(GL_TEXTURE_2D, texture_2d_id_for_copy_);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_2d_id_for_copy);

    gl->DestroyImageCHROMIUM(image_2d_id_for_copy);
  }
}

CanvasResourceGpuMemoryBuffer::~CanvasResourceGpuMemoryBuffer() {
  OnDestroy();
}

bool CanvasResourceGpuMemoryBuffer::IsValid() const {
  return !!context_provider_wrapper_ && image_id_;
}

GLenum CanvasResourceGpuMemoryBuffer::TextureTarget() const {
  return gpu::GetPlatformSpecificTextureTarget();
}

IntSize CanvasResourceGpuMemoryBuffer::Size() const {
  return IntSize(gpu_memory_buffer_->GetSize());
}

scoped_refptr<CanvasResourceGpuMemoryBuffer>
CanvasResourceGpuMemoryBuffer::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    bool is_accelerated) {
  TRACE_EVENT0("blink", "CanvasResourceGpuMemoryBuffer::Create");
  auto resource = AdoptRef(new CanvasResourceGpuMemoryBuffer(
      size, color_params, std::move(context_provider_wrapper),
      std::move(provider), filter_quality, is_accelerated));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceGpuMemoryBuffer::TearDown() {
  // Unref should result in destruction.
  DCHECK(!surface_ || surface_->unique());

  surface_ = nullptr;
  if (context_provider_wrapper_ && image_id_) {
    auto* gl = ContextGL();
    if (gl && image_id_)
      gl->DestroyImageCHROMIUM(image_id_);
    if (gl && texture_id_)
      gl->DeleteTextures(1, &texture_id_);
    if (gl && texture_2d_id_for_copy_)
      gl->DeleteTextures(1, &texture_2d_id_for_copy_);
  }
  image_id_ = 0;
  texture_id_ = 0;
  texture_2d_id_for_copy_ = 0;
  gpu_memory_buffer_ = nullptr;
}

void CanvasResourceGpuMemoryBuffer::Abandon() {
  surface_ = nullptr;
  image_id_ = 0;
  texture_id_ = 0;
  texture_2d_id_for_copy_ = 0;
  gpu_memory_buffer_ = nullptr;
}

const gpu::Mailbox& CanvasResourceGpuMemoryBuffer::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  auto* gl = ContextGL();
  DCHECK(gl);  // caller should already have early exited if !gl.
  if (gpu_mailbox_.IsZero() && gl) {
    gl->ProduceTextureDirectCHROMIUM(texture_id_, gpu_mailbox_.name);
    mailbox_needs_new_sync_token_ = true;
    mailbox_sync_mode_ = sync_mode;
  }
  return gpu_mailbox_;
}

bool CanvasResourceGpuMemoryBuffer::HasGpuMailbox() const {
  return !gpu_mailbox_.IsZero();
}

const gpu::SyncToken CanvasResourceGpuMemoryBuffer::GetSyncToken() {
  if (mailbox_needs_new_sync_token_) {
    auto* gl = ContextGL();
    DCHECK(gl);  // caller should already have early exited if !gl.
    mailbox_needs_new_sync_token_ = false;
    if (mailbox_sync_mode_ == kVerifiedSyncToken) {
      gl->GenSyncTokenCHROMIUM(sync_token_.GetData());
    } else {
      gl->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
    }
  }
  return sync_token_;
}

void CanvasResourceGpuMemoryBuffer::CopyFromTexture(GLuint source_texture,
                                                    GLenum format,
                                                    GLenum type) {
  DCHECK(is_accelerated_);
  if (!IsValid())
    return;

  TRACE_EVENT0("blink", "CanvasResourceGpuMemoryBuffer::CopyFromTexture");
  GLenum target = TextureTarget();
  GLuint texture_id = texture_id_;

  if (texture_2d_id_for_copy_) {
    // We can't CopyTextureCHROMIUM() into a GL_TEXTURE_EXTERNAL_OES; use
    // instead GL_TEXTURE_2D for the CopyTextureChromium().
    target = GL_TEXTURE_2D;
    texture_id = texture_2d_id_for_copy_;
  }

  ContextGL()->CopyTextureCHROMIUM(
      source_texture, 0 /*sourceLevel*/, target, texture_id, 0 /*destLevel*/,
      format, type, false /*unpackFlipY*/, false /*unpackPremultiplyAlpha*/,
      false /*unpackUnmultiplyAlpha*/);

  mailbox_needs_new_sync_token_ = true;
}

void CanvasResourceGpuMemoryBuffer::TakeSkImage(sk_sp<SkImage> image) {
  TRACE_EVENT0("blink", "CanvasResourceGpuMemoryBuffer::CopyFromTexture");

  WillPaint();
  if (!surface_)
    return;
  surface_->getCanvas()->drawImage(image, 0, 0);
  DidPaint();
}

void CanvasResourceGpuMemoryBuffer::WillPaint() {
  if (!IsValid()) {
    surface_ = nullptr;
    return;
  }

  TRACE_EVENT1("blink", "CanvasResourceGpuMemoryBuffer::WillPaint",
               "accelerated", is_accelerated_);

  if (is_accelerated_) {
    if (!surface_) {  // When accelerated it is okay to re-use previous
                      // SkSurface
      GrGLTextureInfo texture_info;
      texture_info.fTarget = TextureTarget();
      texture_info.fID = texture_id_;
      texture_info.fFormat =
          ColorParams().GLSizedInternalFormat();  // unsized format
      GrBackendTexture backend_texture(Size().Width(), Size().Height(),
                                       GrMipMapped::kNo, texture_info);
      constexpr int sample_count = 0;
      surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
          GetGrContext(), backend_texture, kTopLeft_GrSurfaceOrigin,
          sample_count, ColorParams().GetSkColorType(),
          ColorParams().GetSkColorSpace(), nullptr /*surface props*/);
    }
  } else {
    gpu_memory_buffer_->Map();
    void* buffer_base_address = gpu_memory_buffer_->memory(0);
    if (!surface_ || buffer_base_address != buffer_base_address_) {
      buffer_base_address_ = buffer_base_address;
      SkImageInfo image_info = SkImageInfo::Make(
          Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
          ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
      surface_ = SkSurface::MakeRasterDirect(image_info, buffer_base_address_,
                                             gpu_memory_buffer_->stride(0));
    }
  }

  DCHECK(surface_);
}

void CanvasResourceGpuMemoryBuffer::DidPaint() {
  TRACE_EVENT1("blink", "CanvasResourceGpuMemoryBuffer::DidPaint",
               "accelerated", is_accelerated_);

  if (is_accelerated_) {
    auto* gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
    if (gr)
      gr->flush();
    mailbox_needs_new_sync_token_ = true;
  } else {
    gpu_memory_buffer_->Unmap();
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceGpuMemoryBuffer::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceGpuMemoryBuffer::Bitmap() {
  WillPaint();
  scoped_refptr<StaticBitmapImage> bitmap = StaticBitmapImage::Create(
      surface_->makeImageSnapshot(), ContextProviderWrapper());
  DidPaint();
  bitmap->SetOriginClean(is_origin_clean_);
  return bitmap;
}

// CanvasResourceSharedBitmap
//==============================================================================

CanvasResourceSharedBitmap::CanvasResourceSharedBitmap(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      size_(size) {
  shared_memory_ = viz::bitmap_allocation::AllocateMappedBitmap(
      gfx::Size(Size()), ColorParams().TransferableResourceFormat());

  if (!IsValid())
    return;

  shared_bitmap_id_ = viz::SharedBitmap::GenerateId();

  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher) {
    resource_dispatcher->DidAllocateSharedBitmap(
        viz::bitmap_allocation::DuplicateAndCloseMappedBitmap(
            shared_memory_.get(), gfx::Size(Size()),
            ColorParams().TransferableResourceFormat()),
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
}

CanvasResourceSharedBitmap::~CanvasResourceSharedBitmap() {
  OnDestroy();
}

bool CanvasResourceSharedBitmap::IsValid() const {
  return !!shared_memory_;
}

IntSize CanvasResourceSharedBitmap::Size() const {
  return size_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedBitmap::Bitmap() {
  if (!IsValid())
    return nullptr;
  // Construct an SkImage that references the shared memory buffer.
  // The release callback holds a reference to |this| to ensure that the
  // canvas resource that owns the shared memory stays alive at least until
  // the SkImage is destroyed.
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
  SkPixmap pixmap(image_info, shared_memory_->memory(),
                  image_info.minRowBytes());
  this->AddRef();
  sk_sp<SkImage> sk_image = SkImage::MakeFromRaster(
      pixmap,
      [](const void*, SkImage::ReleaseContext resource_to_unref) {
        static_cast<CanvasResourceSharedBitmap*>(resource_to_unref)->Release();
      },
      this);
  auto image = StaticBitmapImage::Create(sk_image);
  image->SetOriginClean(is_origin_clean_);
  return image;
}

scoped_refptr<CanvasResourceSharedBitmap> CanvasResourceSharedBitmap::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  auto resource = AdoptRef(new CanvasResourceSharedBitmap(
      size, color_params, std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceSharedBitmap::TearDown() {
  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher && !shared_bitmap_id_.IsZero()) {
    resource_dispatcher->DidDeleteSharedBitmap(
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
  shared_memory_ = nullptr;
}

void CanvasResourceSharedBitmap::Abandon() {
  shared_memory_ = nullptr;
}

const gpu::Mailbox& CanvasResourceSharedBitmap::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  return shared_bitmap_id_;
}

bool CanvasResourceSharedBitmap::HasGpuMailbox() const {
  return !shared_bitmap_id_.IsZero();
}

void CanvasResourceSharedBitmap::TakeSkImage(sk_sp<SkImage> image) {
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(),
      ColorParams().GetSkColorSpaceForSkSurfaces());

  bool read_pixels_successful = image->readPixels(
      image_info, shared_memory_->memory(), image_info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
}

}  // namespace blink
