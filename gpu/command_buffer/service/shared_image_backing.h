// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_

#include <dawn/webgpu.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android

namespace trace_event {
class ProcessMemoryDump;
class MemoryAllocatorDump;
}  // namespace trace_event
}  // namespace base

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
class MailboxManager;
class SharedContextState;
class SharedImageManager;
class SharedImageRepresentation;
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationGLTexturePassthrough;
class SharedImageRepresentationSkia;
class SharedImageRepresentationDawn;
class SharedImageRepresentationOverlay;
class SharedImageRepresentationVaapi;
class MemoryTypeTracker;
class SharedImageFactory;
class VaapiDependenciesFactory;

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed directly, instead is accessed through a
// SharedImageRepresentation.
class GPU_GLES2_EXPORT SharedImageBacking {
 public:
  SharedImageBacking(const Mailbox& mailbox,
                     viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     GrSurfaceOrigin surface_origin,
                     SkAlphaType alpha_type,
                     uint32_t usage,
                     size_t estimated_size,
                     bool is_thread_safe);

  virtual ~SharedImageBacking();

  viz::ResourceFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  GrSurfaceOrigin surface_origin() const { return surface_origin_; }
  SkAlphaType alpha_type() const { return alpha_type_; }
  uint32_t usage() const { return usage_; }
  const Mailbox& mailbox() const { return mailbox_; }
  size_t estimated_size() const { return estimated_size_; }
  bool is_thread_safe() const { return !!lock_; }
  void OnContextLost();

  // Concrete functions to manage a ref count.
  void AddRef(SharedImageRepresentation* representation);
  void ReleaseRef(SharedImageRepresentation* representation);
  bool HasAnyRefs() const;

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

  // Returns the initialized / cleared region of the SharedImage.
  virtual gfx::Rect ClearedRect() const = 0;

  // Marks the provided rect as cleared.
  virtual void SetClearedRect(const gfx::Rect& cleared_rect) = 0;

  virtual void Update(std::unique_ptr<gfx::GpuFence> in_fence) = 0;

  virtual bool PresentSwapChain();

  virtual void MarkForDestruction() {}

  // Allows the backing to attach additional data to the dump or dump
  // additional sub paths.
  virtual void OnMemoryDump(const std::string& dump_name,
                            base::trace_event::MemoryAllocatorDump* dump,
                            base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t client_tracing_id) {}

  // Prepares the backing for use with the legacy mailbox system.
  // TODO(ericrk): Remove this once the new codepath is complete.
  virtual bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) = 0;

  // Reports the estimated size of the backing for the purpose of memory
  // tracking.
  virtual size_t EstimatedSizeForMemTracking() const;

  // Returns the NativePixmap backing the SharedImageBacking. Returns null if
  // the SharedImage is not backed by a NativePixmap.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap();

#if defined(OS_ANDROID)
  // Returns the AHardwareBuffer from backing if supported and available.
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer();
#endif

  // Helper to determine if the entire SharedImage is cleared.
  bool IsCleared() const { return ClearedRect() == gfx::Rect(size()); }

  // Helper function which clears the entire image.
  void SetCleared() { SetClearedRect(gfx::Rect(size())); }

 protected:
  // Used by SharedImageManager.
  friend class SharedImageManager;
  virtual std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                               MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state);
  virtual std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device);
  virtual std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker);
  virtual std::unique_ptr<SharedImageRepresentationVaapi> ProduceVASurface(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VaapiDependenciesFactory* dep_factory);

  // Used by subclasses during destruction.
  bool have_context() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Used by SharedImageBackingFactoryGLTexture to get register factory.
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
  mutable base::Optional<base::Lock> lock_;

 private:
  class ScopedWriteUMA {
   public:
    ScopedWriteUMA() = default;
    ~ScopedWriteUMA() {
      UMA_HISTOGRAM_BOOLEAN("GPU.SharedImage.ContentConsumed",
                            content_consumed_);
    }

    bool content_consumed() const { return content_consumed_; }
    void SetConsumed() { content_consumed_ = true; }

   private:
    bool content_consumed_ = false;
    DISALLOW_COPY_AND_ASSIGN(ScopedWriteUMA);
  };

  const Mailbox mailbox_;
  const viz::ResourceFormat format_;
  const gfx::Size size_;
  const gfx::ColorSpace color_space_;
  const GrSurfaceOrigin surface_origin_;
  const SkAlphaType alpha_type_;
  const uint32_t usage_;
  const size_t estimated_size_;

  SharedImageFactory* factory_ = nullptr;

  // Bound to the thread on which the backing is created. The |factory_|
  // can only be used from this thread.
  THREAD_CHECKER(factory_thread_checker_);

  bool have_context_ GUARDED_BY(lock_) = true;

  // A scoped object for recording write UMA.
  base::Optional<ScopedWriteUMA> scoped_write_uma_ GUARDED_BY(lock_);

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
  ClearTrackingSharedImageBacking(const Mailbox& mailbox,
                                  viz::ResourceFormat format,
                                  const gfx::Size& size,
                                  const gfx::ColorSpace& color_space,
                                  GrSurfaceOrigin surface_origin,
                                  SkAlphaType alpha_type,
                                  uint32_t usage,
                                  size_t estimated_size,
                                  bool is_thread_safe);

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

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
