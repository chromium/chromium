// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event
}  // namespace base

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
class SharedContextState;
class SharedImageManager;
class SharedImageRepresentation;
class GLTextureImageRepresentation;
class GLTexturePassthroughImageRepresentation;
class SkiaGaneshImageRepresentation;
class SkiaGraphiteImageRepresentation;
class SkiaImageRepresentation;
class DawnImageRepresentation;
class LegacyOverlayImageRepresentation;
class OverlayImageRepresentation;
class MemoryImageRepresentation;
class VaapiImageRepresentation;
class RasterImageRepresentation;
class MemoryTracker;
class VideoDecodeImageRepresentation;
class MemoryTypeTracker;
class SharedImageFactory;
class VaapiDependenciesFactory;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SharedImageBackingType {
  kTest = 0,
  kExternalVkImage = 1,
  kD3D = 2,
  kEGLImage = 3,
  kAHardwareBuffer = 4,
  kAngleVulkan = 5,
  // kGLImage = 6, // no longer used after GLImage removal
  kGLTexture = 7,
  kOzone = 8,
  kRawDraw = 9,
  kSharedMemory = 10,
  kVideo = 11,
  kWrappedSkImage = 12,
  kCompound = 13,
  kDCOMPSurfaceProxy = 14,
  kIOSurface = 15,
  kDCompSurface = 16,
  kDXGISwapChain = 17,
  kWrappedGraphiteTexture = 18,
  kMaxValue = kWrappedGraphiteTexture
};

#if BUILDFLAG(IS_WIN)
using VideoDecodeDevice = Microsoft::WRL::ComPtr<ID3D11Device>;
#else
// This parameter is only used on Windows so null is expected.
using VideoDecodeDevice = void*;
#endif  // BUILDFLAG(IS_WIN)

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed directly, instead is accessed through a
// SharedImageRepresentation.
class GPU_GLES2_EXPORT SharedImageBacking {
 public:
  SharedImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      size_t estimated_size,
      bool is_thread_safe,
      absl::optional<gfx::BufferUsage> buffer_usage = absl::nullopt);

  virtual ~SharedImageBacking();

  viz::SharedImageFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  GrSurfaceOrigin surface_origin() const { return surface_origin_; }
  SkAlphaType alpha_type() const { return alpha_type_; }
  uint32_t usage() const { return usage_; }
  const Mailbox& mailbox() const { return mailbox_; }
  bool is_thread_safe() const { return !!lock_; }
  bool is_ref_counted() const { return is_ref_counted_; }
  gfx::BufferUsage buffer_usage() const { return buffer_usage_.value(); }

  void OnContextLost();

  // Creates SkImageInfo matching backing size, format, alpha and color space
  // for the specified `plane_index`.
  SkImageInfo AsSkImageInfo(int plane_index = 0) const;

  // Disables reference counting for backing. No references should be added,
  // either before or after this is called.
  void SetNotRefCounted();

  // Concrete functions to manage a ref count.
  void AddRef(SharedImageRepresentation* representation);
  void ReleaseRef(SharedImageRepresentation* representation);
  bool HasAnyRefs() const;

  // Returns the memory tracker this backing is registering memory with.
  const MemoryTracker* GetMemoryTracker() const;

  // Notify backing a read access is succeeded
  void OnReadSucceeded();
  // Notify backing a write access is succeeded.
  void OnWriteSucceeded();

  // This factory is registered when creating backing to help
  // create intermediate interop backing buffer
  // and share resource from gl backing buffer to dawn.
  // The factory pointer needs to be reset if the origin
  // factory is destructed. This will handled by destructor of
  // SharedImageRepresentationFactoryRef.
  void RegisterImageFactory(SharedImageFactory* factory);
  void UnregisterImageFactory();

  // Returns string corresponding to GetType() for logging purposes.
  const char* GetName() const;

  virtual SharedImageBackingType GetType() const = 0;

  // Returns the initialized / cleared region of the SharedImage.
  virtual gfx::Rect ClearedRect() const = 0;

  // Marks the provided rect as cleared.
  virtual void SetClearedRect(const gfx::Rect& cleared_rect) = 0;

  // Indicate that the image is purgeable. When an image is purgeable, its
  // contents may be discarded at any time. Before the image can be used again,
  // it must be set to be not-purgeable. This is intended to be lighter-weight
  // than allocating and freeing the image. See investigation in
  // https://crbug.com/1347282.
  virtual void SetPurgeable(bool purgeable) {}
  virtual bool IsPurgeable() const;

  virtual void Update(std::unique_ptr<gfx::GpuFence> in_fence);

  // Uploads pixels from memory into GPU texture. `pixmaps` should have one
  // pixmap per plane. Backings must implement this if they support
  // `SHARED_IMAGE_USAGE_CPU_UPLOAD`.
  virtual bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps);

  // Reads back pixels from GPU texture into memory. `pixmaps` should have one
  // pixmap per plane.
  virtual bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps);

  // Copy from the backing's GPU texture to its GpuMemoryBuffer if present. This
  // is needed on Windows where the renderer process can only create shared
  // memory GMBs and an explicit copy is needed. Returns true on success.
  virtual bool CopyToGpuMemoryBuffer();

  // Present the swap chain corresponding to this backing. Presents only if the
  // backing is the back buffer of the swap chain. Returns true on success.
  virtual bool PresentSwapChain();

  virtual void MarkForDestruction() {}

  // Called when secondary reference is added to the SharedImage. Used by
  // CompoundImageBacking to make sure it can create necessary backings after
  // original ref (and potentially SharedImageFactory) is gone.
  // TODO(vasilyt): We need a better way to make it work for
  // multithreading/multigpu support
  virtual void OnAddSecondaryReference() {}

  // Produces a MemoryAllocatorDump with `dump_name` and creates a shared
  // ownership edge to `client_guid`. Subclasses can extend this function to
  // add additional ownership edges linked to `client_guid` but they must call
  // SharedImageBacking::OnMemoryDump() first (or do something equivalent) to
  // create a MemoryAllocatorDump and ownership edge.
  virtual base::trace_event::MemoryAllocatorDump* OnMemoryDump(
      const std::string& dump_name,
      base::trace_event::MemoryAllocatorDumpGuid client_guid,
      base::trace_event::ProcessMemoryDump* pmd,
      uint64_t client_tracing_id);

  // Gets the estimated size of the backing. This is the value recorded for peak
  // GPU memory tracking.
  size_t GetEstimatedSize() const LOCKS_EXCLUDED(lock_);

  // Reports the estimated size of the backing for the purpose of memory-infra
  // dumps. By default this is the same value reported by GetEstimatedSize().
  // Backings that must query for an accurate size can override this to provide
  // a more accurate estimated size for memory dumps.
  virtual size_t GetEstimatedSizeForMemoryDump() const;

  // Returns the NativePixmap backing the SharedImageBacking. Returns null if
  // the SharedImage is not backed by a NativePixmap.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap();

  // Returns the GpuMemoryBufferHandle if present.
  virtual gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle();

  // Helper to determine if the entire SharedImage is cleared.
  bool IsCleared() const { return ClearedRect() == gfx::Rect(size()); }

  // Marks the entire image as cleared.
  void SetCleared() { SetClearedRect(gfx::Rect(size())); }

 protected:
  // Used by SharedImageManager.
  friend class SharedImageManager;
  friend class CompoundImageBacking;

  virtual std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker);
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state);
  // Returns a SkiaGaneshImageRepresentation created using the Skia Ganesh
  // backend.
  virtual std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state);
  // Returns a SkiaGraphiteImageRepresentation created using the Skia Graphite
  // backend.
  virtual std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state);
  virtual std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats);
  virtual std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<VaapiImageRepresentation> ProduceVASurface(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VaapiDependenciesFactory* dep_factory);
  virtual std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<RasterImageRepresentation> ProduceRaster(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  // Take void* device for resource generated from different devices. E.g  video
  // decoder starts using its own device on a separate thread.
  virtual std::unique_ptr<VideoDecodeImageRepresentation> ProduceVideoDecode(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VideoDecodeDevice device);

#if BUILDFLAG(IS_ANDROID)
  virtual std::unique_ptr<LegacyOverlayImageRepresentation>
  ProduceLegacyOverlay(SharedImageManager* manager, MemoryTypeTracker* tracker);
#endif

  // Updates the estimated size if memory usage changes after creation.
  void UpdateEstimatedSize(size_t estimated_size_bytes)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Used by subclasses during destruction.
  bool have_context() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Used by GLTextureImageBackingFactory to get register factory.
  SharedImageFactory* factory() {
    DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);
    return factory_;
  }

  // Helper class used by subclasses to acquire |lock_| if it exists.
  class SCOPED_LOCKABLE GPU_GLES2_EXPORT AutoLock {
   public:
    explicit AutoLock(const SharedImageBacking* shared_image_backing)
        EXCLUSIVE_LOCK_FUNCTION(shared_image_backing->lock_);
    ~AutoLock() UNLOCK_FUNCTION();

    AutoLock(const AutoLock&) = delete;
    AutoLock& operator=(const AutoLock&) = delete;

    static base::Lock* InitializeLock(
        const SharedImageBacking* shared_image_backing);

   private:
    base::AutoLockMaybe auto_lock_;
  };

  // Protects non-const members here and in derived classes. Protected access
  // to allow GUARDED_BY macros in derived classes. Should not be used
  // directly. Use AutoLock instead.
  mutable absl::optional<base::Lock> lock_;

 private:
  class ScopedWriteUMA {
   public:
    ScopedWriteUMA() = default;

    ScopedWriteUMA(const ScopedWriteUMA&) = delete;
    ScopedWriteUMA& operator=(const ScopedWriteUMA&) = delete;

    ~ScopedWriteUMA() {
      UMA_HISTOGRAM_BOOLEAN("GPU.SharedImage.ContentConsumed",
                            content_consumed_);
    }

    bool content_consumed() const { return content_consumed_; }
    void SetConsumed() { content_consumed_ = true; }

   private:
    bool content_consumed_ = false;
  };

  const Mailbox mailbox_;
  const viz::SharedImageFormat format_;
  const gfx::Size size_;
  const gfx::ColorSpace color_space_;
  const GrSurfaceOrigin surface_origin_;
  const SkAlphaType alpha_type_;
  const uint32_t usage_;
  size_t estimated_size_ GUARDED_BY(lock_);

  // Note that this will be eventually removed and merged into SharedImageUsage.
  const absl::optional<gfx::BufferUsage> buffer_usage_;

  bool is_ref_counted_ = true;

  raw_ptr<SharedImageFactory> factory_ = nullptr;

  // Bound to the thread on which the backing is created. The |factory_|
  // can only be used from this thread.
  THREAD_CHECKER(factory_thread_checker_);

  bool have_context_ GUARDED_BY(lock_) = true;

  // A scoped object for recording write UMA.
  absl::optional<ScopedWriteUMA> scoped_write_uma_ GUARDED_BY(lock_);

  // A vector of SharedImageRepresentations which hold references to this
  // backing. The first reference is considered the owner, and the vector is
  // ordered by the order in which references were taken.
  std::vector<SharedImageRepresentation*> refs_ GUARDED_BY(lock_);
};

// Helper implementation of SharedImageBacking which tracks a simple
// rectangular clear region. Classes which do not need more complex
// implementations of SetClearedRect and ClearedRect can inherit from this.
class GPU_GLES2_EXPORT ClearTrackingSharedImageBacking
    : public SharedImageBacking {
 public:
  ClearTrackingSharedImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      size_t estimated_size,
      bool is_thread_safe,
      absl::optional<gfx::BufferUsage> buffer_usage = absl::nullopt);

  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;

 protected:
  gfx::Rect ClearedRectInternal() const EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void SetClearedRectInternal(const gfx::Rect& cleared_rect)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

 private:
  gfx::Rect cleared_rect_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_H_
