// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "skia/buildflags.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

class GrBackendTexture;
class SkImage;

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace gfx {

class ColorSpace;

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

  // Returns true if the resource can be used with accelerated compositing.
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

  // The bounds for this resource.
  virtual gfx::Size Size() const = 0;

  // Whether this is origin top-left or bottom-left image.
  virtual bool IsOriginTopLeft() const { return true; }

  // Whether this resource uses ClientSharedImage.
  // TODO(crbug.com/351275962): Remove this method once
  // CanvasResourceSharedBitmap holds ClientSharedImage and
  // ExternalCanvasResource either holds ClientSharedImage or is removed.
  virtual bool UsesClientSharedImage() { return false; }

  // The ClientSharedImage containing information on the SharedImage (if any)
  // attached to the resource.
  // NOTE: Valid to call only if UsesClientSharedImage() is true.
  virtual scoped_refptr<gpu::ClientSharedImage> GetClientSharedImage() {
    NOTREACHED();
  }

  // A CanvasResource is not thread-safe and does not allow concurrent usage
  // from multiple threads. But it maybe used from any thread. It remains bound
  // to the current thread until Transfer is called. Note that while the
  // resource maybe used for reads on any thread, it can be written to only on
  // the thread where it was created.
  virtual void Transfer() {}

  // Returns the sync token to indicate when all writes to the current resource
  // are finished on the GPU thread. Note that the token is not guaranteed to be
  // verified at the time of calling this method.
  const gpu::SyncToken GetSyncToken() {
    return GetSyncTokenWithOptionalVerification(false);
  }

  // Provides a TransferableResource representation of this resource to share it
  // with the compositor.
  bool PrepareTransferableResource(viz::TransferableResource*,
                                   ReleaseCallback*,
                                   bool needs_verified_synctoken);

  // Issues a wait for this sync token on the context used by this resource for
  // rendering.
  void WaitSyncToken(const gpu::SyncToken&);

  virtual bool OriginClean() const = 0;
  virtual void SetOriginClean(bool) = 0;

  // Provides a StaticBitmapImage wrapping this resource. Commonly used for
  // snapshots not used in compositing (for instance to draw to another canvas).
  virtual scoped_refptr<StaticBitmapImage> Bitmap() = 0;

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
  // Returns the texture format used by this resource.
  viz::SharedImageFormat GetSharedImageFormat() const;

  SkImageInfo CreateSkImageInfo() const;

  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != owning_thread_ref_;
  }

  virtual bool HasDetailedMemoryDumpProvider() const { return false; }

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>,
                 cc::PaintFlags::FilterQuality,
                 const SkColorInfo&);

  // Returns true if the resource is backed by memory such that it can be used
  // for direct scanout by the display.
  virtual bool IsOverlayCandidate() const { return false; }

  gpu::InterfaceBase* InterfaceBase() const;
  gpu::gles2::GLES2Interface* ContextGL() const;
  gpu::raster::RasterInterface* RasterInterface() const;
  gpu::webgpu::WebGPUInterface* WebGPUInterface() const;
  gfx::ColorSpace GetColorSpace() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const = 0;

  // Prepares GPU TransferableResource from the resource's ClientSharedImage.
  // Invoked if the resource is accelerated and UsesClientSharedImage() returns
  // true.
  bool PrepareAcceleratedTransferableResourceFromClientSI(
      viz::TransferableResource* out_resource,
      bool needs_verified_synctoken);

  // Prepares GPU TransferableResource for resources for which
  // SupportsAcceleratedCompositing() is true but UsesClientSharedImage()
  // returns false. Subclasses that match these conditions *must* override this
  // method.
  virtual bool PrepareAcceleratedTransferableResourceWithoutClientSI(
      viz::TransferableResource* out_resource) {
    NOTREACHED();
  }

  // Prepares software TransferableResource if supported (by default it is not).
  // Subclasses that return false for SupportsAcceleratedCompositing() must
  // override this method to implement support.
  // NOTE: Will be called only if SupportsAcceleratedCompositing() is false.
  virtual bool PrepareUnacceleratedTransferableResource(
      viz::TransferableResource* out_resource) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  const SkColorInfo& GetSkColorInfo() const { return info_; }

  CanvasResourceProvider* Provider() { return provider_.get(); }
  base::WeakPtr<CanvasResourceProvider> WeakProvider() { return provider_; }

  const base::PlatformThreadRef owning_thread_ref_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

 private:
  // Returns true if the resource is rastered via the GPU.
  virtual bool UsesAcceleratedRaster() const = 0;

  // Returns the sync token to indicate when all writes to the current resource
  // are finished on the GPU thread. Note that in some subclasses the token is
  // not guaranteed to be verified at the time of calling this method. Passing
  // true for `needs_verified_token` ensures that the returned token will be
  // verified.
  virtual const gpu::SyncToken GetSyncTokenWithOptionalVerification(
      bool needs_verified_token) {
    NOTREACHED_IN_MIGRATION();
    return gpu::SyncToken();
  }

  base::WeakPtr<CanvasResourceProvider> provider_;
  SkColorInfo info_;
  cc::PaintFlags::FilterQuality filter_quality_;
  LastUnrefCallback last_unref_callback_;
};

// Resource type for SharedBitmaps
class PLATFORM_EXPORT CanvasResourceSharedBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedBitmap> Create(
      const SkImageInfo&,
      base::WeakPtr<CanvasResourceProvider>,
      base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>,
      cc::PaintFlags::FilterQuality);
  ~CanvasResourceSharedBitmap() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsValid() const final;
  bool SupportsAcceleratedCompositing() const final { return false; }
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override {
    return nullptr;
  }
  bool PrepareUnacceleratedTransferableResource(
      viz::TransferableResource* out_resource) final;
  gfx::Size Size() const final;

  // Copies the contents of |image| to the resource's backing memory.
  void TakeSkImage(sk_sp<SkImage> image);

  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool flag) final { is_origin_clean_ = flag; }
  void NotifyResourceLost() override;

 private:
  CanvasResourceSharedBitmap(
      const SkImageInfo&,
      base::WeakPtr<CanvasResourceProvider>,
      base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>,
      cc::PaintFlags::FilterQuality);

  bool UsesAcceleratedRaster() const final { return false; }

  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken sync_token_;
  viz::SharedBitmapId shared_bitmap_id_;
  base::WritableSharedMemoryMapping shared_mapping_;
  gfx::Size size_;
  bool is_origin_clean_ = true;
};

// Resource type for SharedImage
class PLATFORM_EXPORT CanvasResourceSharedImage final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedImage> Create(
      const SkImageInfo&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left,
      bool is_accelerated,
      gpu::SharedImageUsageSet shared_image_usage_flags);
  ~CanvasResourceSharedImage() override;

  bool IsRecycleable() const final { return true; }
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool IsValid() const final;
  gfx::Size Size() const final { return size_; }
  bool IsOriginTopLeft() const final { return is_origin_top_left_; }
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  void Transfer() final;

  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  void NotifyResourceLost() final;
  void BeginWriteAccess();
  void EndWriteAccess();
  GrBackendTexture CreateGrTexture() const;

  GLuint GetTextureIdForReadAccess() const {
    return owning_thread_data().texture_id_for_read_access;
  }
  GLuint GetTextureIdForWriteAccess() const {
    return owning_thread_data().texture_id_for_write_access;
  }

  void WillDraw();
  bool IsLost() const { return owning_thread_data().is_lost; }
  void CopyRenderingResultsToGpuMemoryBuffer(const sk_sp<SkImage>& image);
  bool UsesClientSharedImage() override { return true; }
  scoped_refptr<gpu::ClientSharedImage> GetClientSharedImage() override;
  const scoped_refptr<gpu::ClientSharedImage>& GetClientSharedImage() const;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    const std::string& parent_path,
                    size_t bytes_per_pixel) const;
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
      scoped_refptr<CanvasResourceSharedImage> resource,
      bool has_read_ref_on_texture,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::SyncToken GetSyncTokenWithOptionalVerification(
      bool needs_verified_token) override;
  bool IsOverlayCandidate() const final { return is_overlay_candidate_; }
  bool UsesAcceleratedRaster() const final { return is_accelerated_; }

  CanvasResourceSharedImage(const SkImageInfo&,
                            base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                            base::WeakPtr<CanvasResourceProvider>,
                            cc::PaintFlags::FilterQuality,
                            bool is_origin_top_left,
                            bool is_accelerated,
                            gpu::SharedImageUsageSet shared_image_usage_flags);

  OwningThreadData& owning_thread_data() {
    DCHECK(!is_cross_thread());
    return owning_thread_data_;
  }
  const OwningThreadData& owning_thread_data() const {
    DCHECK(!is_cross_thread());
    return owning_thread_data_;
  }

  // Can be read on any thread.

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

  // Accessed on any thread.
  const gfx::Size size_;
  const bool is_origin_top_left_;
  const bool is_accelerated_;
  const bool is_overlay_candidate_;
  const bool supports_display_compositing_;
  const bool use_oop_rasterization_;
  OwningThreadData owning_thread_data_;
};

// Resource type for a given opaque external resource described on construction
// via a TransferableResource. This CanvasResource should only be used with
// context that support GL.
class PLATFORM_EXPORT ExternalCanvasResource final : public CanvasResource {
 public:
  // Creates ExternalCanvasResource with a ClientSharedImage instance whose
  // mailbox matches that of `transferable_resource`.
  // TODO(crbug.com/353744937): Remove this class taking in
  // TransferableResource:
  // * Ensure that it would be correct for CanvasResource to go into all
  //   relevant UsesClientSharedImage() codepaths for ExternalCanvasResource
  //   (in particular, PrepareAcceleratedTransferableResourceFromClientSI())
  // * Change ExternalCanvasResource to take *only* the ClientSharedImage and
  //   override UsesClientSharedImage() to return true
  static scoped_refptr<ExternalCanvasResource> Create(
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const viz::TransferableResource& transferable_resource,
      viz::ReleaseCallback release_callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      cc::PaintFlags::FilterQuality,
      bool is_origin_top_left);

  ~ExternalCanvasResource() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  gfx::Size Size() const final { return transferable_resource_.size; }
  bool IsOriginTopLeft() const final { return is_origin_top_left_; }
  void NotifyResourceLost() override { resource_is_lost_ = true; }

  scoped_refptr<StaticBitmapImage> Bitmap() override;
  viz::ReleaseCallback TakeVizReleaseCallback() override {
    return std::move(release_callback_);
  }
  void SetVizReleaseCallback(viz::ReleaseCallback cb) override {
    release_callback_ = std::move(cb);
  }

 private:
  bool IsOverlayCandidate() const final {
    return transferable_resource_.is_overlay_candidate;
  }
  bool UsesAcceleratedRaster() const final { return true; }
  const gpu::SyncToken GetSyncTokenWithOptionalVerification(
      bool needs_verified_token) override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  bool PrepareAcceleratedTransferableResourceWithoutClientSI(
      viz::TransferableResource* out_resource) override;
  void GenOrFlushSyncToken();

  ExternalCanvasResource(scoped_refptr<gpu::ClientSharedImage> client_si,
                         const viz::TransferableResource& transferable_resource,
                         viz::ReleaseCallback out_callback,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceProvider>,
                         cc::PaintFlags::FilterQuality,
                         bool is_origin_top_left);

  scoped_refptr<gpu::ClientSharedImage> client_si_;
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
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  gfx::Size Size() const final { return size_; }
  void NotifyResourceLost() override {
    // Used for single buffering mode which doesn't need to care about sync
    // token synchronization.
  }

  scoped_refptr<StaticBitmapImage> Bitmap() override;

  GLuint GetBackBufferTextureId() const { return back_buffer_texture_id_; }
  scoped_refptr<gpu::ClientSharedImage> GetBackBufferClientSharedImage() {
    CHECK(back_buffer_shared_image_);
    return back_buffer_shared_image_;
  }
  void PresentSwapChain();
  bool UsesClientSharedImage() override { return true; }
  scoped_refptr<gpu::ClientSharedImage> GetClientSharedImage() override;

 private:
  bool IsOverlayCandidate() const final { return true; }
  bool UsesAcceleratedRaster() const final { return true; }
  const gpu::SyncToken GetSyncTokenWithOptionalVerification(
      bool needs_verified_token) override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;

  CanvasResourceSwapChain(const SkImageInfo&,
                          base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                          base::WeakPtr<CanvasResourceProvider>,
                          cc::PaintFlags::FilterQuality);

  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  const gfx::Size size_;
  scoped_refptr<gpu::ClientSharedImage> front_buffer_shared_image_;
  scoped_refptr<gpu::ClientSharedImage> back_buffer_shared_image_;
  GLuint back_buffer_texture_id_ = 0u;
  gpu::SyncToken sync_token_;
  const bool use_oop_rasterization_;

  bool is_origin_clean_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
