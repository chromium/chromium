// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/mailbox.mojom-blink.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace base {

class SharedMemory;

}  // namespace base

namespace viz {

class SingleReleaseCallback;
struct TransferableResource;

}  // namespace viz

namespace blink {

class CanvasResourceProvider;
class StaticBitmapImage;

// TODO(danakj): One day the gpu::mojom::Mailbox type should be shared with
// blink directly and we won't need to use gpu::mojom::blink::Mailbox, nor the
// conversion through WTF::Vector.
gpu::mojom::blink::MailboxPtr SharedBitmapIdToGpuMailboxPtr(
    const viz::SharedBitmapId& id);

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
class PLATFORM_EXPORT CanvasResource
    : public WTF::ThreadSafeRefCounted<CanvasResource> {
 public:
  virtual ~CanvasResource();
  virtual void Abandon() { TearDown(); }
  virtual bool IsRecycleable() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool SupportsAcceleratedCompositing() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool NeedsReadLockFences() const { return false; }
  virtual IntSize Size() const = 0;
  virtual const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) = 0;
  virtual void Transfer() {}
  virtual const gpu::SyncToken GetSyncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  bool PrepareTransferableResource(viz::TransferableResource*,
                                   std::unique_ptr<viz::SingleReleaseCallback>*,
                                   MailboxSyncMode);
  void WaitSyncToken(const gpu::SyncToken&);
  virtual scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) = 0;
  virtual scoped_refptr<CanvasResource> MakeUnaccelerated() = 0;
  virtual bool OriginClean() const = 0;
  virtual void SetOriginClean(bool) = 0;
  virtual scoped_refptr<StaticBitmapImage> Bitmap() = 0;
  virtual void CopyFromTexture(GLuint source_texture,
                               GLenum format,
                               GLenum type) {
    NOTREACHED();
  }

  // Only CanvasResourceProvider and derivatives should call this.
  virtual void TakeSkImage(sk_sp<SkImage> image) = 0;

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>,
                 SkFilterQuality,
                 const CanvasColorParams&);

  virtual GLenum TextureTarget() const {
    NOTREACHED();
    return 0;
  }
  virtual bool IsOverlayCandidate() const { return false; }
  virtual bool HasGpuMailbox() const = 0;
  virtual void TearDown() = 0;
  gpu::gles2::GLES2Interface* ContextGL() const;
  GLenum GLFilter() const;
  GrContext* GetGrContext() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const {
    NOTREACHED();
    return nullptr;
  }
  bool PrepareAcceleratedTransferableResource(
      viz::TransferableResource* out_resource,
      MailboxSyncMode);
  bool PrepareUnacceleratedTransferableResource(
      viz::TransferableResource* out_resource);
  SkFilterQuality FilterQuality() const { return filter_quality_; }
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void OnDestroy();
  CanvasResourceProvider* Provider() { return provider_.get(); }

 private:
  // Sync token that was provided when resource was released
  gpu::SyncToken sync_token_for_release_;
  base::WeakPtr<CanvasResourceProvider> provider_;
  SkFilterQuality filter_quality_;
  CanvasColorParams color_params_;
  blink::PlatformThreadId thread_of_origin_;
#if DCHECK_IS_ON()
  bool did_call_on_destroy_ = false;
#endif
};

// Resource type for skia Bitmaps (RAM and texture backed)
class PLATFORM_EXPORT CanvasResourceBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceBitmap> Create(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      const CanvasColorParams&);
  ~CanvasResourceBitmap() override;

  // Not recyclable: Skia handles texture recycling internally and bitmaps are
  // cheap to allocate.
  bool IsRecycleable() const final { return false; }
  bool IsAccelerated() const final;
  bool SupportsAcceleratedCompositing() const override {
    return IsAccelerated();
  }
  bool IsValid() const final;
  IntSize Size() const final;
  void Transfer() final;
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final;
  void SetOriginClean(bool value) final;
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final;
  scoped_refptr<CanvasResource> MakeUnaccelerated() final;
  void TakeSkImage(sk_sp<SkImage> image) final;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;

  CanvasResourceBitmap(scoped_refptr<StaticBitmapImage>,
                       base::WeakPtr<CanvasResourceProvider>,
                       SkFilterQuality,
                       const CanvasColorParams&);

  scoped_refptr<StaticBitmapImage> image_;
};

// Resource type for GpuMemoryBuffers
class PLATFORM_EXPORT CanvasResourceGpuMemoryBuffer final
    : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceGpuMemoryBuffer> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      bool is_accelerated);
  ~CanvasResourceGpuMemoryBuffer() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return is_accelerated_; }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool NeedsReadLockFences() const final { return true; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  };
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final;
  void TakeSkImage(sk_sp<SkImage> image) final;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override;
  scoped_refptr<StaticBitmapImage> Bitmap() override;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  bool IsOverlayCandidate() const final { return true; }
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void WillPaint();
  void DidPaint();

  CanvasResourceGpuMemoryBuffer(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      bool is_accelerated);

  gpu::Mailbox gpu_mailbox_;
  gpu::SyncToken sync_token_;
  bool mailbox_needs_new_sync_token_ = false;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  void* buffer_base_address_ = nullptr;
  sk_sp<SkSurface> surface_;
  GLuint image_id_ = 0;
  GLuint texture_id_ = 0;
  MailboxSyncMode mailbox_sync_mode_ = kVerifiedSyncToken;
  bool is_accelerated_;
  bool is_origin_clean_ = true;

  // GL_TEXTURE_2D view of |gpu_memory_buffer_| for CopyFromTexture(); only used
  // if TextureTarget() is GL_TEXTURE_EXTERNAL_OES.
  GLuint texture_2d_id_for_copy_ = 0;
};

// Resource type for SharedBitmaps
class PLATFORM_EXPORT CanvasResourceSharedBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedBitmap> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  ~CanvasResourceSharedBitmap() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return false; }
  bool IsValid() const final;
  bool SupportsAcceleratedCompositing() const final { return false; }
  bool NeedsReadLockFences() const final { return false; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  };
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final;
  void TakeSkImage(sk_sp<SkImage> image) final;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override {
    NOTREACHED();
  }
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool flag) final { is_origin_clean_ = flag; }

 private:
  void TearDown() override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;

  CanvasResourceSharedBitmap(const IntSize&,
                             const CanvasColorParams&,
                             base::WeakPtr<CanvasResourceProvider>,
                             SkFilterQuality);

  viz::SharedBitmapId shared_bitmap_id_;
  std::unique_ptr<base::SharedMemory> shared_memory_;
  IntSize size_;
  bool is_origin_clean_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
