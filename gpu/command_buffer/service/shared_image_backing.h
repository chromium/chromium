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
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace base {
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
class MemoryTypeTracker;

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed direclty, instead is accessed through a
// SharedImageRepresentation.
class GPU_GLES2_EXPORT SharedImageBacking {
 public:
  SharedImageBacking(const Mailbox& mailbox,
                     viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     uint32_t usage,
                     size_t estimated_size,
                     bool is_thread_safe);

  virtual ~SharedImageBacking();

  viz::ResourceFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  uint32_t usage() const { return usage_; }
  const Mailbox& mailbox() const { return mailbox_; }
  size_t estimated_size() const { return estimated_size_; }
  void OnContextLost();

  // Concrete functions to manage a ref count.
  void AddRef(SharedImageRepresentation* representation);
  void ReleaseRef(SharedImageRepresentation* representation);
  bool HasAnyRefs() const;

  // Notify backing a read access is succeeded
  void OnReadSucceeded();
  // Notify backing a write access is succeeded.
  void OnWriteSucceeded();

  // Tracks whether the backing has ever been cleared, or whether it may contain
  // uninitialized pixels.
  virtual bool IsCleared() const = 0;

  // Marks the backing as cleared, after which point it is assumed to contain no
  // unintiailized pixels.
  virtual void SetCleared() = 0;

  virtual void Update(std::unique_ptr<gfx::GpuFence> in_fence) = 0;

  // Destroys the underlying backing. Must be called before destruction.
  virtual void Destroy() = 0;

  virtual bool PresentSwapChain();

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

  // Used by subclasses in Destroy.
  bool have_context() const;

  void AssertLockedIfNecessary() const;

  class GPU_GLES2_EXPORT AutoLock {
   public:
    explicit AutoLock(const SharedImageBacking* shared_image_backing);
    ~AutoLock();

    AutoLock(const AutoLock&) = delete;
    AutoLock& operator=(const AutoLock&) = delete;

    static base::Lock* InitializeLock(
        const SharedImageBacking* shared_image_backing);

   private:
    base::AutoLockMaybe auto_lock_;
  };

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
  const uint32_t usage_;
  const size_t estimated_size_;

  // Protects non-const members here and in derived classes.
  mutable base::Optional<base::Lock> lock_;

  bool have_context_ = true;

  // A scoped object for recording write UMA.
  base::Optional<ScopedWriteUMA> scoped_write_uma_;

  // A vector of SharedImageRepresentations which hold references to this
  // backing. The first reference is considered the owner, and the vector is
  // ordered by the order in which references were taken.
  std::vector<SharedImageRepresentation*> refs_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
