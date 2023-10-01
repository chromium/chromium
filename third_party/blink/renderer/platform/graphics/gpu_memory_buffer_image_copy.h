#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

class Image;

class PLATFORM_EXPORT GpuMemoryBufferImageCopy {
  USING_FAST_MALLOC(GpuMemoryBufferImageCopy);

 public:
  GpuMemoryBufferImageCopy(gpu::gles2::GLES2Interface*,
                           gpu::SharedImageInterface*);
  ~GpuMemoryBufferImageCopy();

  // SyncToken will be completed after GpuMemoryBuffer access is finished by
  // GPU process.
  std::pair<gfx::GpuMemoryBuffer*, gpu::SyncToken> CopyImage(Image*);

 private:
  bool EnsureDestImage(const gfx::Size&);
  void CleanupDestImage();

  const raw_ptr<gpu::gles2::GLES2Interface, ExperimentalRenderer> gl_;
  const raw_ptr<gpu::SharedImageInterface, ExperimentalRenderer> sii_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Size dest_image_size_;
  gpu::Mailbox dest_mailbox_;

  // TODO(billorr): Add error handling for context loss or GL errors before we
  // enable this by default.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_
