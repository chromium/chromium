// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedImageRepresentationFactoryRef;
class SharedImageBatchAccessManager;
class VaapiDependenciesFactory;

class GPU_GLES2_EXPORT SharedImageManager {
 public:
  // If |thread_safe| is set, the manager itself can be safely accessed from
  // other threads but the backings themselves may not be thread-safe so
  // representations should not be created on other threads. When
  // |display_context_on_another_thread| is set, we make sure that all
  // SharedImages that will be used in the display context have thread-safe
  // backings and therefore it is safe to create representations on the thread
  // that holds the display context.
  explicit SharedImageManager(bool thread_safe = false,
                              bool display_context_on_another_thread = false);
  ~SharedImageManager();

  // Registers a SharedImageBacking with the manager and returns a
  // SharedImageRepresentationFactoryRef which holds a ref on the SharedImage.
  // The factory should delete this object to release the ref.
  std::unique_ptr<SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<SharedImageBacking> backing,
      MemoryTypeTracker* ref);

  // Marks the backing associated with a mailbox as context lost.
  void OnContextLost(const Mailbox& mailbox);

  // Accessors which return a SharedImageRepresentation. Representations also
  // take a ref on the mailbox, releasing it when the representation is
  // destroyed.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(const Mailbox& mailbox, MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(const Mailbox& mailbox, MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      scoped_refptr<SharedContextState> context_state);
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      WGPUDevice device);
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationVaapi> ProduceVASurface(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      VaapiDependenciesFactory* dep_factory);
  std::unique_ptr<SharedImageRepresentationMemory> ProduceMemory(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);

  // Called by SharedImageRepresentation in the destructor.
  void OnRepresentationDestroyed(const Mailbox& mailbox,
                                 SharedImageRepresentation* representation);

  // Dump memory for the given mailbox.
  void OnMemoryDump(const Mailbox& mailbox,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);

  bool is_thread_safe() const { return !!lock_; }

  bool display_context_on_another_thread() const {
    return display_context_on_another_thread_;
  }

  // Returns the NativePixmap backing |mailbox|. Returns null if the SharedImage
  // doesn't exist or is not backed by a NativePixmap. The caller is not
  // expected to read from or write into the provided NativePixmap because it
  // can be modified by the client at any time. The primary purpose of this
  // method is to facilitate pageflip testing on the viz thread.
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(const gpu::Mailbox& mailbox);

  SharedImageBatchAccessManager* batch_access_manager() const {
#if defined(OS_ANDROID)
    return batch_access_manager_.get();
#else
    return nullptr;
#endif
  }

  bool BeginBatchReadAccess();
  bool EndBatchReadAccess();

 private:
  class AutoLock;
  // The lock for protecting |images_|.
  base::Optional<base::Lock> lock_;

  base::flat_set<std::unique_ptr<SharedImageBacking>> images_ GUARDED_BY(lock_);

  const bool display_context_on_another_thread_;

#if defined(OS_ANDROID)
  std::unique_ptr<SharedImageBatchAccessManager> batch_access_manager_;
#endif

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SharedImageManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
