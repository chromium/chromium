// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class GPU_EXPORT ClientSharedImage
    : public base::RefCountedThreadSafe<ClientSharedImage> {
 public:
  explicit ClientSharedImage(const Mailbox& mailbox);
  ClientSharedImage(const Mailbox& mailbox,
                    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer);

  const Mailbox& mailbox() { return mailbox_; }

  gfx::GpuMemoryBuffer* gpu_memory_buffer() { return gpu_memory_buffer_.get(); }

  base::trace_event::MemoryAllocatorDumpGuid GetGUIDForTracing() {
    return gpu::GetSharedImageGUIDForTracing(mailbox_);
  }

  static scoped_refptr<ClientSharedImage> CreateForTesting() {
    return base::MakeRefCounted<ClientSharedImage>(
        Mailbox::GenerateForSharedImage());
  }

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  ~ClientSharedImage();

  const Mailbox mailbox_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
