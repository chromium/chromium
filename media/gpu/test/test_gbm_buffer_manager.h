// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_TEST_GBM_BUFFER_MANAGER_H_
#define MEDIA_GPU_TEST_TEST_GBM_BUFFER_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/ipc/common/surface_handle.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/linux/scoped_gbm_device.h"

namespace gfx {
struct NativePixmapHandle;
class Size;
}  // namespace gfx

namespace media {

class TestGbmBuffer;

class MEDIA_GPU_EXPORT TestGbmBufferManager {
 public:
  TestGbmBufferManager();

  TestGbmBufferManager(const TestGbmBufferManager&) = delete;
  TestGbmBufferManager& operator=(const TestGbmBufferManager&) = delete;

  ~TestGbmBufferManager();

  std::unique_ptr<TestGbmBuffer> CreateGbmBuffer(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event);

  // Imports a DmaBuf as a TestGbmBuffer to be able to map it. The
  // GBM_BO_USE_SW_READ_OFTEN usage is specified so that the user of the
  // returned TestGbmBuffer is guaranteed to have a linear view when mapping
  // it.
  std::unique_ptr<TestGbmBuffer> ImportDmaBuf(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format);

  // Returns true if the combination of |format| and |usage| is supported by
  // CreateTestGbmBuffer().
  bool IsFormatAndUsageSupported(viz::SharedImageFormat format,
                                 gfx::BufferUsage usage);

 private:
  ui::ScopedGbmDevice gbm_device_;
};

class TestGbmBuffer {
 public:
  TestGbmBuffer() = delete;

  TestGbmBuffer(gbm_bo* buffer_object);

  TestGbmBuffer(const TestGbmBuffer&) = delete;
  TestGbmBuffer& operator=(const TestGbmBuffer&) = delete;

  ~TestGbmBuffer();

  bool Map();
  void* memory(size_t plane);
  void Unmap();
  gfx::Size GetSize() const;
  int stride(size_t plane) const;
  gfx::GpuMemoryBufferHandle CloneHandle() const;

 private:
  struct MappedPlane {
    raw_ptr<void> addr;
    raw_ptr<void> mapped_data;
  };

  raw_ptr<gbm_bo> buffer_object_;
  gfx::GpuMemoryBufferHandle handle_;
  bool mapped_;
  std::vector<MappedPlane> mapped_planes_;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_TEST_GBM_BUFFER_MANAGER_H_
