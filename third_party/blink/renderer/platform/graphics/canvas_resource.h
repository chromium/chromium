// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "skia/buildflags.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

class GrDirectContext;
class GrBackendTexture;
class SkImage;

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace gfx {

class ColorSpace;
class GpuMemoryBuffer;

}  // namespace gfx

namespace gpu::raster {

class RasterInterface;

}  // namespace gpu::raster

namespace blink {

class CanvasResourceProvider;
class StaticBitmapImage;

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
// Note that this object may be accessed across multiple threads but not
// concurrently. The caller is responsible to call Transfer on the object before
// using it on a different thread.
class PLATFORM_EXPORT CanvasResource
    : public WTF::ThreadSafeRefCounted<CanvasResource> {
 public:
  using ReleaseCallback = base::OnceCallback<void(
      scoped_refptr<blink::CanvasResource>&& canvas_resource,
      const gpu::SyncToken& sync_token,
      bool is_lost)>;

  using LastUnrefCallback = base::OnceCallback<void(
      scoped_refptr<blink::CanvasResource> canvas_resource)>;

  virtual ~CanvasResource();

  // Non-virtual override of ThreadSafeRefCounted::Release
  void Release();

  // Set a callback that will be invoked as the last outstanding reference to
  // this CanvasResource goes out of scope.  This provides a last chance hook
  // to intercept a canvas before it get destroyed. For resources that need to
  // be destroyed on their thread of origin, this hook can be used to return
  // resources to their creators.
  void SetLastUnrefCallback(LastUnrefCallback callback) {
    last_unref_callback_ = std::move(callback);
  }

  bool HasLastUnrefCallback() { return !!last_unref_callback_; }

  // We perform a lazy copy on write if the canvas content needs to be updated
  // while its current resource is in use. In order to avoid re-allocating
  // resources, its preferable to reuse a resource if its no longer in use.
  // This API indicates whether a resource can be recycled.  This method does
  // not however check whether the resource is still in use (e.g. has
  // outstanding references).
  virtual bool IsRecycleable() const = 0;

  // Returns true if rendering to the resource is accelerated.
  virtual bool IsAccelerated() const = 0;

  // Returns true if the resource can be used with accelerated compositing. This
  // is different from IsAccelerated since a resource may be rendered to on the
  // CPU but can be used with GPU compositing (using GMBs).
  virtual bool SupportsAcceleratedCompositing() const = 0;

  // Transfers ownership of the resource's vix::ReleaseCallback.  This is useful
  // prior to transferring a resource to another thread, to retain the release
  // callback on the current thread since the callback may not be thread safe.
  // Even if the callback is never executed on another thread, simply transiting
  // through another thread is dangerous because garbage collection races may
  // make it impossible to return the resource to its thread of origin for
  // destruction; in which case the callback (and its bound arguments) may be
  // destroyed on the wrong thread.
  virtual viz::ReleaseCallback TakeVizReleaseCallback() {
    return viz::ReleaseCallback();
  }

  virtual void SetVizReleaseCallback(viz::ReleaseCallback cb) {
    CHECK(cb.is_null());
  }

  // Returns true if the resource is still usable. It maybe not be valid in the
  // case of a context loss or if we fail to initialize the memory backing for
  // the resource.
  virtual bool IsValid() const = 0;

  // When a resource is returned by the display compositor, a sync token is
  // provided to indicate when the compositor's commands using the resource are
  // executed on the GPU thread.
  // However in some cases we need to ensure that the commands using the
  // resource have finished executing on the GPU itself. This API indicates
  // whether this is required. The primary use-case for this is GMBs rendered to
  // on the CPU but composited on the GPU. Its important for the GPU reads to be
  // finished before updating the resource on the CPU.
  virtual bool NeedsReadLockFences() const { return false; }

  // The bounds for this resource.
  virtual gfx::Size Size() const = 0;

  // Whether this is origin top-left or bottom-left image.
  virtual bool IsOriginTopLeft() const { return true; }

  // The mailbox which can be used to reference this resource in GPU commands.
  // The sync mode indicates how the sync token for the resource should be
  // prepared.
  virtual const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) = 0;

  // A CanvasResource is not thread-safe and does not allow concurrent usage
  // from multiple threads. But it maybe used from any thread. It remains bound
  // to the current thread until Transfer is called. Note that while the
  // resource maybe used for reads on any thread, it can be written to only on
  // the thread where it was created.
  virtual void Transfer() {}

  // Returns the sync token to indicate when all writes to the current resource
  // are finished on the GPU thread.
  virtual const gpu::SyncToken GetSyncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }

  // Provides a TransferableResource representation of this resource to share it
  // with the compositor.
  bool PrepareTransferableResource(viz::TransferableResource*,
                                   ReleaseCallback*,
                                   MailboxSyncMode);

  // Issues a wait for this sync token on the context used by this resource for
  // rendering.
  void WaitSyncToken(const gpu::SyncToken&);

  virtual bool OriginClean() const = 0;
  virtual void SetOriginClean(bool) = 0;

  // Provides a StaticBitmapImage wrapping this resource. Commonly used for
  // snapshots not used in compositing (for instance to draw to another canvas).
  virtual scoped_refptr<StaticBitmapImage> Bitmap() = 0;

  // Copies the contents of |image| to the resource's backing memory. Only
  // CanvasResourceProvider and derivatives should call this.
  virtual void TakeSkImage(sk_sp<SkImage> image) = 0;

  // Called when the resource is marked lost. Losing a resource does not mean
  // that the backing memory has been destroyed, since the resource itself keeps
  // a ref on that memory.
  // It means that the consumer (commonly the compositor) can not provide a sync
  // token for the resource to be safely recycled and its the GL state may be
  // inconsistent with when the resource was given to the compositor. So it
  // should not be recycled for writing again but can be safely read from.
  virtual void NotifyResourceLost() = 0;

  void SetFilterQuality(cc::PaintFlags::FilterQuality filter) {
    filter_quality_ = filter;
  }
  // The filter quality to use when the resource is drawn by the compositor.
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }

  SkImageInfo CreateSkImageInfo() const;

  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != owning_thread_ref_;
  }
  // Returns the texture target for the resource.
  virtual GLenum TextureTarget() const {
    NOTREACHED();
    return 0;
  }

  virtual bool HasDetailedMemoryDumpProvider() const { return false; }

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>,
                 cc::PaintFlags::FilterQuality,
                 const SkColorInfo&);

  // Called during resource destruction if the resource is destroyed on a thread
  // other than where it was created. This implies that no context associated
  // cleanup can be done and any resources tied to the context may be leaked. As
  // such, a resource must be deleted on the owning thread and this should only
  // be called when the owning thread and its associated context was torn down
  // before this resource could be deleted.
  virtual void Abandon() { TearDown(); }

  // Returns true if the resource is backed by memory such that it can be used
  // for direct scanout by the display.
  virtual bool IsOverlayCandidate() const { return false; }

  // Returns true if the resource is backed by memory that can be referenced
  // using a mailbox.
  virtual bool HasGpuMailbox() const = 0;

  // Destroys the backing memory and any other references to it kept alive by
  // this object. This must be called from the same thread where the resource
  // was created.
  virtual void TearDown() = 0;

  gpu::InterfaceBase* InterfaceBase() const;
  gpu::gles2::GLES2Interface* ContextGL() const;
  gpu::raster::RasterInterface* RasterInterface() const;
  gpu::webgpu::WebGPUInterface* WebGPUInterface() const;
  viz::SharedImageFormat GetSharedImageFormat() const;
  gfx::BufferFormat GetBufferFormat() const;
  gfx::ColorSpace GetColorSpace() const;
  GrDirectContext* GetGrContext() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const {
    NOTREACHED();
    return nullptr;
  }
  virtual bool PrepareAcceleratedTransferableResource(
      viz::TransferableResource* out_resource,
      MailboxSyncMode);
  bool PrepareUnacceleratedTransferableResource(
      viz::TransferableResource* out_resource);
  const SkColorInfo& GetSkColorInfo() const { return info_; }
  void OnDestroy();
  CanvasResourceProvider* Provider() { return provider_.get(); }
  base::WeakPtr<CanvasResourceProvider> WeakProvider() { return provider_; }

  const base::PlatformThreadRef owning_thread_ref_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

 private:
  base::WeakPtr<CanvasResourceProvider> provider_;
  SkColorInfo info_;
  cc::PaintFlags::FilterQuality filter_quality_;
  LastUnrefCallback last_unref_callback_;
#if DCHECK_IS_ON()
  bool did_call_on_destroy_ = false;
#endif
};

// Resource type for SharedBitmaps
class PLATFORM_EXPORT CanvasResourceSharedBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedBitmap> Create(
      const SkImageInfo&,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality);
  ~CanvasResourceSharedBitmap() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return false; }
  bool IsValid() const final;
  bool SupportsAcceleratedCompositing() const final { return false; }
  bool NeedsReadLockFences() const final { return false; }
  void Abandon() final;
  gfx::Size Size() const final;
  void TakeSkImage(sk_sp<SkImage> image) final;
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool flag) final { is_origin_clean_ = flag; }
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  void NotifyResourceLost() override;

 private:
  void TearDown() override;
  bool HasGpuMailbox() const override;

  CanvasResourceSharedBitmap(const SkImageInfo&,
                             base::WeakPtr<CanvasResourceProvider>,
                             cc::PaintFlags::FilterQuality);

  viz::SharedBitmapId shared_bitmap_id_;
  base::WritableSharedMemoryMapping shared_mapping_;
  gfx::Size size_;
  bool is_origin_clean_ = true;
};

// Intermediate class for all SharedImage implementations.
class PLATFORM_EXPORT CanvasResourceSharedImage : public CanvasResource {
 public:
  virtual void BeginReadAccess() = 0;
  virtual void EndReadAccess() = 0;
  virtual void BeginWriteAccess() = 0;
  virtual void EndWriteAccess() = 0;
  virtual GrBackendTexture CreateGrTexture() const = 0;
  virtual void WillDraw() = 0;
  virtual bool HasReadAccess() const = 0;
  virtual bool IsLost() const = 0;
  virtual void CopyRenderingResultsToGpuMemoryBuffer(const sk_sp<SkImage>&) = 0;
  virtual void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                            size_t bytes_per_pixel) const {}

 protected:
  CanvasResourceSharedImage(base::WeakPtr<CanvasResourceProvider>,
                            cc::PaintFlags::FilterQuality,
                            const SkColorInfo&);
};

// Resource type for Raster-based SharedImage
class PLATFORM_EXPORT CanvasResourceRasterSharedImage final
    : public CanvasResourceSharedImage {
 public:
  static scoped_refptr<CanvasResourceRasterSharedImage> Create(
      const SkImageInfo&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left,
      bool is_accelerated,
      uint32_t shared_image_usage_flags);
  ~CanvasResourceRasterSharedImage() override;

  bool IsRecycleable() const final { return true; }
  bool IsAccelerated() const final { return is_accelerated_; }
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool IsValid() const final;
  gfx::Size Size() const final { return size_; }
  bool IsOriginTopLeft() const final { return is_origin_top_left_; }
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  void Transfer() final;

  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  void TakeSkImage(sk_sp<SkImage> image) final { NOTREACHED(); }
  void NotifyResourceLost() final;
  bool NeedsReadLockFences() const final {
    // If the resource is not accelerated, it will be written to on the CPU. We
    // need read lock fences to ensure that all reads on the GPU are done when
    // the resource is returned by the display compositor.
    return !is_accelerated_;
  }
  void BeginReadAccess() final;
  void EndReadAccess() final;
  void BeginWriteAccess() final;
  void EndWriteAccess() final;
  GrBackendTexture CreateGrTexture() const final;

  GLuint GetTextureIdForReadAccess() const {
    return owning_thread_data().texture_id_for_read_access;
  }
  GLuint GetTextureIdForWriteAccess() const {
    return owning_thread_data().texture_id_for_write_access;
  }
  GLenum TextureTarget() const override { return texture_target_; }

  void WillDraw() final;
  bool HasReadAccess() const final {
    return owning_thread_data().bitmap_image_read_refs > 0u;
  }
  bool IsLost() const final { return owning_thread_data().is_lost; }
  void CopyRenderingResultsToGpuMemoryBuffer(const sk_sp<SkImage>& image) final;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    size_t bytes_per_pixel) const override;
  // Whether this type of CanvasResource can provide detailed memory data. If
  // true, then the CanvasResourceProvider will not report data, to avoid
  // double-countintg.
  bool HasDetailedMemoryDumpProvider() const override { return true; }

 private:
  // These members are either only accessed on the owning thread, or are only
  // updated on the owning thread and then are read on a different thread.
  // We ensure to correctly update their state in Transfer, which is called
  // before a resource is used on a different thread.
  struct OwningThreadData {
    bool mailbox_needs_new_sync_token = true;
    scoped_refptr<gpu::ClientSharedImage> client_shared_image;
    gpu::SyncToken sync_token;
    size_t bitmap_image_read_refs = 0u;
    MailboxSyncMode mailbox_sync_mode = kUnverifiedSyncToken;
    bool is_lost = false;

    // We need to create 2 representations if canvas is operating in single
    // buffered mode to allow concurrent scopes for read and write access,
    // because the Begin/EndSharedImageAccessDirectCHROMIUM APIs allow only one
    // active access mode for a representation.
    // In non single buffered mode, the 2 texture ids are the same.
    GLuint texture_id_for_read_access = 0u;
    GLuint texture_id_for_write_access = 0u;
  };

  static void OnBitmapImageDestroyed(
      scoped_refptr<CanvasResourceRasterSharedImage> resource,
      bool has_read_ref_on_texture,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  void TearDown() override;
  void Abandon() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  bool IsOverlayCandidate() const final { return is_overlay_candidate_; }

  CanvasResourceRasterSharedImage(
      const SkImageInfo&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left,
      bool is_accelerated,
      uint32_t shared_image_usage_flags);

  OwningThreadData& owning_thread_data() {
    DCHECK(!is_cross_thread());
    return owning_thread_data_;
  }
  const OwningThreadData& owning_thread_data() const {
    DCHECK(!is_cross_thread());
    return owning_thread_data_;
  }

  // Can be read on any thread.
  gpu::ClientSharedImage* client_shared_image() const {
    return owning_thread_data_.client_shared_image.get();
  }
  bool mailbox_needs_new_sync_token() const {
    return owning_thread_data_.mailbox_needs_new_sync_token;
  }
  const gpu::SyncToken& sync_token() const {
    return owning_thread_data_.sync_token;
  }

  // This should only be de-referenced on the owning thread but may be copied
  // on a different thread.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

  // This can be accessed on any thread, irrespective of whether there are
  // active readers or not.
  bool is_origin_clean_ = true;

  // GMB based software raster path. The resource is written to on the CPU but
  // passed using the mailbox to the display compositor for use as an overlay.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // Accessed on any thread.
  const gfx::Size size_;
  const bool is_origin_top_left_;
  const bool is_accelerated_;
  const bool is_overlay_candidate_;
  const bool supports_display_compositing_;
  const GLenum texture_target_;
  const bool use_oop_rasterization_;
  // TODO(crbug.com/1494911): Remove this field once GetOrCreateGpuMailbox() is
  // converted to return ClientSharedImage.
  const gpu::Mailbox empty_mailbox_;
  OwningThreadData owning_thread_data_;
};

// Resource type for a given opaque external resource described on construction
// via a Mailbox; this CanvasResource IsAccelerated() by definition.
// This resource can also encapsulate an external mailbox, synctoken and release
// callback, exported from WebGL. This CanvasResource should only be used with
// context that support GL.
class PLATFORM_EXPORT ExternalCanvasResource final : public CanvasResource {
 public:
  static scoped_refptr<ExternalCanvasResource> Create(
      const viz::TransferableResource& transferable_resource,
      viz::ReleaseCallback release_callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left);

  static scoped_refptr<ExternalCanvasResource> Create(
      const gpu::Mailbox& mailbox,
      viz::ReleaseCallback release_callback,
      gpu::SyncToken sync_token,
      const SkImageInfo&,
      GLenum texture_target,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left,
      bool is_overlay_candidate);

  ~ExternalCanvasResource() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return true; }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool NeedsReadLockFences() const final { return false; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  void Abandon() final;
  gfx::Size Size() const final { return transferable_resource_.size; }
  bool IsOriginTopLeft() const final { return is_origin_top_left_; }
  void TakeSkImage(sk_sp<SkImage> image) final;
  void NotifyResourceLost() override { resource_is_lost_ = true; }

  scoped_refptr<StaticBitmapImage> Bitmap() override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  viz::ReleaseCallback TakeVizReleaseCallback() override {
    return std::move(release_callback_);
  }
  void SetVizReleaseCallback(viz::ReleaseCallback cb) override {
    release_callback_ = std::move(cb);
  }

 private:
  void TearDown() override;
  GLenum TextureTarget() const final {
    return transferable_resource_.mailbox_holder.texture_target;
  }
  bool IsOverlayCandidate() const final {
    return transferable_resource_.is_overlay_candidate;
  }
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  bool PrepareAcceleratedTransferableResource(
      viz::TransferableResource* out_resource,
      MailboxSyncMode) override;
  void GenOrFlushSyncToken();

  ExternalCanvasResource(const viz::TransferableResource& transferable_resource,
                         viz::ReleaseCallback out_callback,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceProvider>,
                         cc::PaintFlags::FilterQuality,
                         bool is_origin_top_left);

  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  viz::TransferableResource transferable_resource_;
  viz::ReleaseCallback release_callback_;
  const bool is_origin_top_left_;
  bool is_origin_clean_ = true;
  bool resource_is_lost_ = false;
};

class PLATFORM_EXPORT CanvasResourceSwapChain final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSwapChain> Create(
      const SkImageInfo&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality);
  ~CanvasResourceSwapChain() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return true; }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool NeedsReadLockFences() const final { return false; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  void Abandon() final;
  gfx::Size Size() const final { return size_; }
  void TakeSkImage(sk_sp<SkImage> image) final;
  void NotifyResourceLost() override {
    // Used for single buffering mode which doesn't need to care about sync
    // token synchronization.
  }

  scoped_refptr<StaticBitmapImage> Bitmap() override;

  GLenum TextureTarget() const final { return GL_TEXTURE_2D; }

  GLuint GetBackBufferTextureId() const { return back_buffer_texture_id_; }
  const gpu::Mailbox& GetBackBufferMailbox() { return back_buffer_mailbox_; }

  void PresentSwapChain();
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;

 private:
  void TearDown() override;
  bool IsOverlayCandidate() const final { return true; }
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;

  CanvasResourceSwapChain(const SkImageInfo&,
                          base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                          base::WeakPtr<CanvasResourceProvider>,
                          cc::PaintFlags::FilterQuality);

  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  const gfx::Size size_;
  gpu::Mailbox front_buffer_mailbox_;
  gpu::Mailbox back_buffer_mailbox_;
  GLuint back_buffer_texture_id_ = 0u;
  gpu::SyncToken sync_token_;
  const bool use_oop_rasterization_;

  bool is_origin_clean_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
