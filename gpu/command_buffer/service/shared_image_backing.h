// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_

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

namespace gpu {
class MailboxManager;
class SharedImageManager;
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationGLTexturePassthrough;
class SharedImageRepresentationSkia;

// Represents the actual storage (GL texture, VkImage, GMB) for a SharedImage.
// Should not be accessed direclty, instead is accessed through a
// SharedImageRepresentation.
class GPU_GLES2_EXPORT SharedImageBacking {
 public:
  SharedImageBacking(const Mailbox& mailbox,
                     viz::ResourceFormat format,
                     const gfx::Size& size,
                     const gfx::ColorSpace& color_space,
                     uint32_t usage);

  virtual ~SharedImageBacking();

  viz::ResourceFormat format() const { return format_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  uint32_t usage() const { return usage_; }
  const Mailbox& mailbox() const { return mailbox_; }
  void OnContextLost() { have_context_ = false; }

  // Tracks whether the backing has ever been cleared, or whether it may contain
  // uninitialized pixels.
  virtual bool IsCleared() const = 0;

  // Marks the backing as cleared, after which point it is assumed to contain no
  // unintiailized pixels.
  virtual void SetCleared() = 0;

  // Destroys the underlying backing. Must be called before destruction.
  virtual void Destroy() = 0;

  // Memory dump helpers:
  // Returns the estimated size of the backing. If 0 is returned, the dump will
  // be omitted.
  virtual size_t EstimatedSize() const;
  // Allows the backing to attach additional data to the dump or dump
  // additional sub paths.
  virtual void OnMemoryDump(const std::string& dump_name,
                            base::trace_event::MemoryAllocatorDump* dump,
                            base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t client_tracing_id) {}

  // Prepares the backing for use with the legacy mailbox system.
  // TODO(ericrk): Remove this once the new codepath is complete.
  virtual bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) = 0;

 protected:
  // Used by SharedImageManager.
  friend class SharedImageManager;
  virtual std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager);
  virtual std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager);
  virtual std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager);

  // Used by subclasses in Destroy.
  bool have_context() const { return have_context_; }

 private:
  const Mailbox mailbox_;
  const viz::ResourceFormat format_;
  const gfx::Size size_;
  const gfx::ColorSpace color_space_;
  const uint32_t usage_;

  bool have_context_ = true;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_H_
