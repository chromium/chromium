#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_

#include <memory>
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

class Image;

class PLATFORM_EXPORT GpuMemoryBufferImageCopy {
  USING_FAST_MALLOC(GpuMemoryBufferImageCopy);

 public:
  GpuMemoryBufferImageCopy(gpu::gles2::GLES2Interface*);
  ~GpuMemoryBufferImageCopy();

  gfx::GpuMemoryBuffer* CopyImage(Image*);

 private:
  bool EnsureMemoryBuffer(int width, int height);
  void OnContextLost();
  void OnContextError(const char* msg, int32_t id);

  std::unique_ptr<gfx::GpuMemoryBuffer> m_currentBuffer;

  int last_width_ = 0;
  int last_height_ = 0;
  gpu::gles2::GLES2Interface* gl_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // TODO(billorr): Add error handling for context loss or GL errors before we
  // enable this by default.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_MEMORY_BUFFER_IMAGE_COPY_H_
