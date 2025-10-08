// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/test_gbm_buffer_manager.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

namespace {

const int32_t kDrmNumNodes = 64;
const int32_t kMinNodeNumber = 128;

// TODO(crbug.com/40115082): use ui/gfx/linux/gbm_device.h instead.
gbm_device* CreateGbmDevice() {
  int fd;
  int32_t min_node = kMinNodeNumber;
  int32_t max_node = kMinNodeNumber + kDrmNumNodes;
  struct gbm_device* gbm = nullptr;

  for (int i = min_node; i < max_node; i++) {
    fd = drmOpenRender(i);
    if (fd < 0) {
      continue;
    }

    drmVersionPtr version = drmGetVersion(fd);
    if (!strcmp("vgem", version->name)) {
      drmFreeVersion(version);
      close(fd);
      continue;
    }

    gbm = gbm_create_device(fd);
    if (!gbm) {
      drmFreeVersion(version);
      close(fd);
      continue;
    }

    VLOG(1) << "Opened gbm device on render node " << version->name;
    drmFreeVersion(version);
    return gbm;
  }

  return nullptr;
}

uint32_t GetDrmFormat(viz::SharedImageFormat si_format) {
  if (si_format == viz::SinglePlaneFormat::kR_8) {
    return DRM_FORMAT_R8;
  }
  if (si_format == viz::MultiPlaneFormat::kYV12) {
    return DRM_FORMAT_YVU420;
  }
  if (si_format == viz::MultiPlaneFormat::kNV12) {
    return DRM_FORMAT_NV12;
  }
  // Add more formats when needed.
  return 0;
}

uint32_t GetGbmUsage(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_READ |
             GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_SW_READ_OFTEN;
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_READ |
             GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_ENCODER | GBM_BO_USE_SW_READ_OFTEN;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SW_READ_OFTEN;
    default:
      return 0;
  }
}

}  // namespace

TestGbmBuffer::TestGbmBuffer(gbm_bo* buffer_object)
    : buffer_object_(buffer_object), mapped_(false) {
  gfx::NativePixmapHandle native_pixmap_handle;
  for (size_t i = 0;
       i < static_cast<size_t>(gbm_bo_get_plane_count(buffer_object)); ++i) {
    native_pixmap_handle.planes.push_back(gfx::NativePixmapPlane(
        gbm_bo_get_stride_for_plane(buffer_object, i),
        gbm_bo_get_offset(buffer_object, i),
        gbm_bo_get_plane_size(buffer_object, i),
        base::ScopedFD(gbm_bo_get_plane_fd(buffer_object, i))));
  }
  handle_ = gfx::GpuMemoryBufferHandle(std::move(native_pixmap_handle));
}

TestGbmBuffer::~TestGbmBuffer() {
  if (mapped_) {
    Unmap();
  }

  gbm_bo_destroy(buffer_object_.ExtractAsDangling());
}

bool TestGbmBuffer::Map() {
  if (mapped_) {
    return true;
  }
  size_t num_planes = gbm_bo_get_plane_count(buffer_object_);
  uint32_t stride;
  mapped_planes_.resize(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    void* mapped_data;
    void* addr =
        gbm_bo_map2(buffer_object_, 0, 0, gbm_bo_get_width(buffer_object_),
                    gbm_bo_get_height(buffer_object_),
                    GBM_BO_TRANSFER_READ_WRITE, &stride, &mapped_data, i);
    if (addr == MAP_FAILED) {
      LOG(ERROR) << "Failed to map TestGbmBuffer plane " << i;
      Unmap();
      return false;
    }
    mapped_planes_[i].addr = addr;
    mapped_planes_[i].mapped_data = mapped_data;
  }
  mapped_ = true;
  return true;
}

void* TestGbmBuffer::memory(size_t plane) {
  if (!mapped_) {
    LOG(ERROR) << "Buffer is not mapped";
    return nullptr;
  }
  if (plane > mapped_planes_.size()) {
    LOG(ERROR) << "Invalid plane: " << plane;
    return nullptr;
  }
  return mapped_planes_[plane].addr;
}

void TestGbmBuffer::Unmap() {
  for (size_t i = 0; i < mapped_planes_.size(); ++i) {
    if (mapped_planes_[i].addr) {
      gbm_bo_unmap(buffer_object_, mapped_planes_[i].mapped_data);
      mapped_planes_[i].addr = nullptr;
      mapped_planes_[i].mapped_data = nullptr;
    }
  }
  mapped_planes_.clear();
  mapped_ = false;
}

gfx::Size TestGbmBuffer::GetSize() const {
  return gfx::Size(gbm_bo_get_width(buffer_object_),
                   gbm_bo_get_height(buffer_object_));
}

int TestGbmBuffer::stride(size_t plane) const {
  return gbm_bo_get_stride_for_plane(buffer_object_, plane);
}

gfx::GpuMemoryBufferHandle TestGbmBuffer::CloneHandle() const {
  DCHECK_EQ(handle_.type, gfx::NATIVE_PIXMAP);
  gfx::GpuMemoryBufferHandle handle(
      gfx::CloneHandleForIPC(handle_.native_pixmap_handle()));
  return handle;
}

TestGbmBufferManager::TestGbmBufferManager() : gbm_device_(CreateGbmDevice()) {}
TestGbmBufferManager::~TestGbmBufferManager() = default;

std::unique_ptr<TestGbmBuffer> TestGbmBufferManager::CreateGbmBuffer(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  if (!gbm_device_.get()) {
    LOG(ERROR) << "Invalid GBM device";
    return nullptr;
  }

  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format) {
    LOG(ERROR) << "Unable to convert viz::SharedImageFormat "
               << format.ToString() << " to DRM format";
    return nullptr;
  }

  const uint32_t gbm_usage = GetGbmUsage(usage);
  if (gbm_usage == 0) {
    LOG(ERROR) << "Unsupported usage " << gfx::BufferUsageToString(usage);
    return nullptr;
  }

  if (!gbm_device_is_format_supported(gbm_device_.get(), drm_format,
                                      gbm_usage)) {
    return nullptr;
  }

  gbm_bo* buffer_object = gbm_bo_create(gbm_device_.get(), size.width(),
                                        size.height(), drm_format, gbm_usage);
  if (!buffer_object) {
    LOG(ERROR) << "Failed to create GBM buffer object";
    return nullptr;
  }

  return std::make_unique<TestGbmBuffer>(buffer_object);
}

std::unique_ptr<TestGbmBuffer> TestGbmBufferManager::ImportDmaBuf(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  if (handle.planes.size() != static_cast<size_t>(format.NumberOfPlanes())) {
    // This could happen if e.g., we get a compressed RGBA buffer where one
    // plane is for metadata. We don't support this case.
    LOG(ERROR) << "Cannot import " << format.ToString() << " with "
               << handle.planes.size() << " plane(s) (expected "
               << format.NumberOfPlanes() << " plane(s))";
    return nullptr;
  }
  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format) {
    LOG(ERROR) << "Unsupported format " << format.ToString();
    return nullptr;
  }
  gbm_import_fd_modifier_data import_data{
      base::checked_cast<uint32_t>(size.width()),
      base::checked_cast<uint32_t>(size.height()), drm_format,
      base::checked_cast<uint32_t>(handle.planes.size())};
  for (size_t plane = 0; plane < handle.planes.size(); plane++) {
    if (!handle.planes[plane].fd.is_valid()) {
      LOG(ERROR) << "Invalid file descriptor for plane " << plane;
      return nullptr;
    }
    import_data.fds[plane] = handle.planes[plane].fd.get();
    import_data.strides[plane] =
        base::checked_cast<int>(handle.planes[plane].stride);
    import_data.offsets[plane] =
        base::checked_cast<int>(handle.planes[plane].offset);
  }
  import_data.modifier = handle.modifier;
  gbm_bo* buffer_object =
      gbm_bo_import(gbm_device_.get(), GBM_BO_IMPORT_FD_MODIFIER, &import_data,
                    GBM_BO_USE_SW_READ_OFTEN);
  if (!buffer_object) {
    PLOG(ERROR) << "Could not import the DmaBuf into gbm";
    return nullptr;
  }
  return std::make_unique<TestGbmBuffer>(buffer_object);
}

bool TestGbmBufferManager::IsFormatAndUsageSupported(
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format) {
    return false;
  }
  const uint32_t gbm_usage = GetGbmUsage(usage);
  if (gbm_usage == 0) {
    return false;
  }
  return gbm_device_is_format_supported(gbm_device_.get(), drm_format,
                                        gbm_usage);
}

}  // namespace media
