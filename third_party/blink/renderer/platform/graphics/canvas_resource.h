// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/release_callback.h"
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
#include "third_party/blink/renderer/platform/graphics/canvas_high_entropy_op_type.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

class SkSurface;

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace gfx {

class ColorSpace;

}  // namespace gfx

namespace gpu::raster {

class RasterInterface;

}  // namespace gpu::raster

namespace blink {

class CanvasResourceProviderSharedImage;
class StaticBitmapImage;

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
// Note that this object may be accessed across multiple threads but not
// concurrently. The caller is responsible to call Transfer on the object before
// using it on a different thread.
class PLATFORM_EXPORT CanvasResource
    : public ThreadSafeRefCounted<CanvasResource> {
 public:
  using ReleaseCallback = base::OnceCallback<void(
      scoped_refptr<blink::CanvasResource>&& canvas_resource,
      const gpu::SyncToken& sync_token,
      bool is_lost)>;

  using LastUnrefCallback = base::OnceCallback<void(
      scoped_refptr<blink::CanvasResource> canvas_resource)>;

  virtual ~CanvasResource();

  static void OnPlaceholderReleasedResource(
      scoped_refptr<CanvasResource> resource);

  // Returns true if this instance creates TransferableResources for usage with
  // GPU compositing.
  virtual bool CreatesAcceleratedTransferableResources() const = 0;

  virtual void OnRefReturned(scoped_refptr<CanvasResource>&& resource) {}

  // Returns true if the resource is still usable. It maybe not be valid in the
  // case of a context loss or if we fail to initialize the memory backing for
  // the resource.
  virtual bool IsValid() const = 0;

  // The bounds for this resource.
  gfx::Size Size() const { return GetClientSharedImage()->size(); }
  base::ByteSize EstimatedSizeInBytes() const {
    return GetClientSharedImage()->EstimatedSizeInBytes();
  }

  // The ClientSharedImage containing information on the SharedImage
  // attached to the resource.
  virtual const scoped_refptr<gpu::ClientSharedImage>& GetClientSharedImage()
      const = 0;

  // A CanvasResource is not thread-safe and does not allow concurrent usage
  // from multiple threads. But it maybe used from any thread. It remains bound
  // to the current thread until Transfer is called. Note that while the
  // resource maybe used for reads on any thread, it can be written to only on
  // the thread where it was created.
  virtual void Transfer() {}

  // Provides a TransferableResource representation of this resource to share it
  // with the compositor.
  bool PrepareTransferableResource(viz::TransferableResource*,
                                   ReleaseCallback*,
                                   bool needs_verified_synctoken);

  // Issues a wait for this sync token on the context used by this resource for
  // rendering.
  virtual void WaitSyncToken(const gpu::SyncToken&) = 0;

  bool OriginClean() const { return is_origin_clean_; }
  void SetOriginClean(bool flag) { is_origin_clean_ = flag; }

  HighEntropyCanvasOpType HighEntropyCanvasOpTypes() const {
    return high_entropy_canvas_op_types_;
  }
  void SetHighEntropyCanvasOpTypes(HighEntropyCanvasOpType types) {
    high_entropy_canvas_op_types_ = types;
  }

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

  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != owning_thread_ref_;
  }

 protected:
  CanvasResource();

  virtual gfx::HDRMetadata GetHDRMetadata() const { return gfx::HDRMetadata(); }
  virtual viz::TransferableResource::ResourceSource
  GetTransferableResourceSource() const {
    return viz::TransferableResource::ResourceSource::kCanvas;
  }

  gpu::InterfaceBase* InterfaceBase() const;
  gpu::gles2::GLES2Interface* ContextGL() const;
  gpu::raster::RasterInterface* RasterInterface() const;
  gpu::webgpu::WebGPUInterface* WebGPUInterface() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const = 0;

  const base::PlatformThreadRef owning_thread_ref_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

 private:
  friend class CanvasResourceProviderTest;
  friend class WebGPUMailboxTexture;

  static void OnPlaceholderReleasedResourceOnOwningThread(
      scoped_refptr<CanvasResource> resource);

  // Returns true if the resource is rastered via the GPU.
  virtual bool UsesAcceleratedRaster() const = 0;

  // Verify the sync token that indicates when all writes to the current
  // resource are finished on the GPU thread. Note that in some subclasses the
  // token is already verified by GetSyncToken() so this function is no-op for
  // those classes.
  virtual void VerifySyncToken() {}
  virtual const gpu::SyncToken& sync_token() const = 0;

  bool is_origin_clean_ = true;
  HighEntropyCanvasOpType high_entropy_canvas_op_types_ =
      HighEntropyCanvasOpType::kNone;
};

// Resource type for SharedImage
class PLATFORM_EXPORT CanvasResourceSharedImage final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedImage> CreateSoftware(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<CanvasResourceProviderSharedImage>,
      base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>);

  static scoped_refptr<CanvasResourceSharedImage> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProviderSharedImage>,
      bool is_accelerated,
      gpu::SharedImageUsageSet shared_image_usage_flags);
  ~CanvasResourceSharedImage() override;

  bool CreatesAcceleratedTransferableResources() const override {
    return !GetClientSharedImage()->is_software();
  }
  void OnRefReturned(scoped_refptr<CanvasResource>&& resource) final;
  bool IsValid() const final;
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  void Transfer() final;

  // Save (and wait on) this sync token on the context used by this resource for
  // rendering.
  // TODO(crbug.com/40286368): completely defer the waiting to the
  // zero-parameter variant of WaitSyncToken().
  void WaitSyncToken(const gpu::SyncToken&) override;

  std::unique_ptr<gpu::RasterScopedAccess> BeginAccess(bool readonly);
  void EndAccess(std::unique_ptr<gpu::RasterScopedAccess> access);

  void NotifyResourceLost() final;

  bool IsLost() const { return owning_thread_data().is_lost; }

  const scoped_refptr<gpu::ClientSharedImage>& GetClientSharedImage()
      const override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    const std::string& parent_path) const;

  SkImageInfo CreateSkImageInfo() const;

  // Signals that an external write has completed, passing the token that should
  // be waited on to ensure that the service-side operations of the external
  // write have completed. Ensures that the next read of this resource (whether
  // via raster or the compositor) waits on this token.
  void EndExternalWrite(const gpu::SyncToken& external_write_sync_token);

  // Uploads the contents of |sk_surface| to the resource's backing memory.
  // Should be called only if the resource is using software raster.
  void UploadSoftwareRenderingResults(SkSurface* sk_surface);

  void PrepareForWebGPUDummyMailbox();

 private:
  friend class CanvasResourceProviderSharedImage;

  // These members are either only accessed on the owning thread, or are only
  // updated on the owning thread and then are read on a different thread.
  // We ensure to correctly update their state in Transfer, which is called
  // before a resource is used on a different thread.
  struct OwningThreadData {
    scoped_refptr<gpu::ClientSharedImage> client_shared_image;
    gpu::SyncToken sync_token;
    bool is_lost = false;
  };

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void VerifySyncToken() override;
  bool UsesAcceleratedRaster() const final { return is_accelerated_; }

  CanvasResourceProviderSharedImage* Provider();

  CanvasResourceSharedImage(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      base::WeakPtr<CanvasResourceProviderSharedImage>,
      base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>);

  CanvasResourceSharedImage(gfx::Size size,
                            viz::SharedImageFormat format,
                            SkAlphaType alpha_type,
                            const gfx::ColorSpace& color_space,
                            base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                            base::WeakPtr<CanvasResourceProviderSharedImage>,
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

  const gpu::SyncToken& sync_token() const override {
    return owning_thread_data_.sync_token;
  }

  SkAlphaType GetAlphaType() const { return alpha_type_; }

  // This should only be de-referenced on the owning thread but may be copied
  // on a different thread.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  gpu::SyncToken acquire_sync_token_;

  // Accessed on any thread.
  const bool is_accelerated_;
  const SkAlphaType alpha_type_;
  OwningThreadData owning_thread_data_;
  base::WeakPtr<CanvasResourceProviderSharedImage> provider_;
};

// Resource type for a given opaque external resource described on construction
// via a TransferableResource. This CanvasResource should only be used with
// context that support GL.
class PLATFORM_EXPORT ExternalCanvasResource final : public CanvasResource {
 public:
  static scoped_refptr<ExternalCanvasResource> Create(
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const gpu::SyncToken& sync_token,
      viz::TransferableResource::ResourceSource resource_source,
      gfx::HDRMetadata hdr_metadata,
      viz::ReleaseCallback release_callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>);

  ~ExternalCanvasResource() override;
  bool IsValid() const override;
  bool CreatesAcceleratedTransferableResources() const override { return true; }
  void NotifyResourceLost() override { resource_is_lost_ = true; }
  const scoped_refptr<gpu::ClientSharedImage>& GetClientSharedImage()
      const final {
    return client_si_;
  }
  void WaitSyncToken(const gpu::SyncToken&) override;
  void GetSyncToken();

  scoped_refptr<StaticBitmapImage> Bitmap() override;

 private:
  gfx::HDRMetadata GetHDRMetadata() const final { return hdr_metadata_; }
  viz::TransferableResource::ResourceSource GetTransferableResourceSource()
      const final {
    return resource_source_;
  }
  bool UsesAcceleratedRaster() const final { return true; }
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void VerifySyncToken() override;

  ExternalCanvasResource(
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const gpu::SyncToken& sync_token,
      viz::TransferableResource::ResourceSource resource_source,
      gfx::HDRMetadata hdr_metadata,
      viz::ReleaseCallback out_callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>);

  SkAlphaType GetAlphaType() const { return alpha_type_; }
  const gpu::SyncToken& sync_token() const override { return sync_token_; }

  scoped_refptr<gpu::ClientSharedImage> client_si_;
  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  gpu::SyncToken sync_token_;
  viz::TransferableResource::ResourceSource resource_source_;
  gfx::HDRMetadata hdr_metadata_;
  viz::ReleaseCallback release_callback_;
  bool resource_is_lost_ = false;
  const SkAlphaType alpha_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
