// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_

#include "base/containers/flat_set.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class GPU_GLES2_EXPORT SharedImageManager {
 public:
  SharedImageManager();
  ~SharedImageManager();

  // Registers a SharedImageBacking with the manager and returns true on
  // success. On success, the backing has one ref which may be released by
  // calling Unregister.
  bool Register(std::unique_ptr<SharedImageBacking> backing);

  // Releases the registration ref. If a backing reaches zero refs, it is
  // destroyed.
  void Unregister(const Mailbox& mailbox);

  // Marks the backing associated with a mailbox as context lost.
  void OnContextLost(const Mailbox& mailbox);

  // Indicates whether a mailbox is associated with a SharedImage.
  // TODO: Remove this once all mailboxes are SharedImages.
  bool IsSharedImage(const Mailbox& mailbox);

  // Accessors which return a SharedImageRepresentation. Representations also
  // take a ref on the mailbox, releasing it when the representation is
  // destroyed.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      const Mailbox& mailbox);
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(const Mailbox& mailbox);
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      const Mailbox& mailbox);

  // Called by SharedImageRepresentation in the destructor.
  void OnRepresentationDestroyed(const Mailbox& mailbox);

  // Dump memory for the given mailbox.
  void OnMemoryDump(const Mailbox& mailbox,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);

 private:
  struct BackingAndRefCount {
    BackingAndRefCount(std::unique_ptr<SharedImageBacking> backing,
                       uint32_t ref_count);
    BackingAndRefCount(BackingAndRefCount&& other);
    BackingAndRefCount& operator=(BackingAndRefCount&& rhs);
    ~BackingAndRefCount();
    std::unique_ptr<SharedImageBacking> backing;
    uint32_t ref_count = 0;
  };
  friend bool operator<(const BackingAndRefCount& lhs,
                        const BackingAndRefCount& rhs);
  friend bool operator<(const Mailbox& lhs, const BackingAndRefCount& rhs);
  friend bool operator<(const BackingAndRefCount& lhs, const Mailbox& rhs);

  base::flat_set<BackingAndRefCount> images_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
