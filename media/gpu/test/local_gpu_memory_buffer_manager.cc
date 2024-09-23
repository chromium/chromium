// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/local_gpu_memory_buffer_manager.h"

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
#include "ui/gfx/buffer_format_util.h"
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

uint32_t GetDrmFormat(gfx::BufferFormat gfx_format) {
  switch (gfx_format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    // Add more formats when needed.
    default:
      return 0;
  }
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

class GpuMemoryBufferImplGbm : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImplGbm() = delete;

  GpuMemoryBufferImplGbm(gfx::BufferFormat format, gbm_bo* buffer_object)
      : format_(format), buffer_object_(buffer_object), mapped_(false) {
    handle_.type = gfx::NATIVE_PIXMAP;
    // Set a dummy id since this is for testing only.
    handle_.id = gfx::GpuMemoryBufferId(0);

    for (size_t i = 0;
         i < static_cast<size_t>(gbm_bo_get_plane_count(buffer_object)); ++i) {
      handle_.native_pixmap_handle.planes.push_back(gfx::NativePixmapPlane(
          gbm_bo_get_stride_for_plane(buffer_object, i),
          gbm_bo_get_offset(buffer_object, i),
          gbm_bo_get_plane_size(buffer_object, i),
          base::ScopedFD(gbm_bo_get_plane_fd(buffer_object, i))));
    }
  }

  GpuMemoryBufferImplGbm(const GpuMemoryBufferImplGbm&) = delete;
  GpuMemoryBufferImplGbm& operator=(const GpuMemoryBufferImplGbm&) = delete;

  ~GpuMemoryBufferImplGbm() override {
    if (mapped_) {
      Unmap();
    }

    gbm_bo_destroy(buffer_object_.ExtractAsDangling());
  }

  bool Map() override {
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
        LOG(ERROR) << "Failed to map GpuMemoryBufferImplGbm plane " << i;
        Unmap();
        return false;
      }
      mapped_planes_[i].addr = addr;
      mapped_planes_[i].mapped_data = mapped_data;
    }
    mapped_ = true;
    return true;
  }

  void* memory(size_t plane) override {
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

  void Unmap() override {
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

  gfx::Size GetSize() const override {
    return gfx::Size(gbm_bo_get_width(buffer_object_),
                     gbm_bo_get_height(buffer_object_));
  }

  gfx::BufferFormat GetFormat() const override { return format_; }

  int stride(size_t plane) const override {
    return gbm_bo_get_stride_for_plane(buffer_object_, plane);
  }

  gfx::GpuMemoryBufferId GetId() const override { return handle_.id; }

  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::NATIVE_PIXMAP;
  }

  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    DCHECK_EQ(handle_.type, gfx::NATIVE_PIXMAP);
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::NATIVE_PIXMAP;
    handle.id = handle_.id;
    handle.native_pixmap_handle =
        gfx::CloneHandleForIPC(handle_.native_pixmap_handle);
    return handle;
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
    auto shared_buffer_guid = gfx::GetGenericSharedGpuMemoryGUIDForTracing(
        tracing_process_id, GetId());
    pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
    pmd->AddOwnershipEdge(buffer_dump_guid, shared_buffer_guid, importance);
  }

 private:
  struct MappedPlane {
    raw_ptr<void> addr;
    raw_ptr<void> mapped_data;
  };

  gfx::BufferFormat format_;
  raw_ptr<gbm_bo> buffer_object_;
  gfx::GpuMemoryBufferHandle handle_;
  bool mapped_;
  std::vector<MappedPlane> mapped_planes_;
};

}  // namespace

LocalGpuMemoryBufferManager::LocalGpuMemoryBufferManager()
    : gbm_device_(CreateGbmDevice()) {}
LocalGpuMemoryBufferManager::~LocalGpuMemoryBufferManager() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
LocalGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  if (!gbm_device_.get()) {
    LOG(ERROR) << "Invalid GBM device";
    return nullptr;
  }

  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format) {
    LOG(ERROR) << "Unable to convert gfx::BufferFormat "
               << static_cast<int>(format) << " to DRM format";
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

  return std::make_unique<GpuMemoryBufferImplGbm>(format, buffer_object);
}

void LocalGpuMemoryBufferManager::CopyGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

bool LocalGpuMemoryBufferManager::CopyGpuMemoryBufferSync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  return false;
}

std::unique_ptr<gfx::GpuMemoryBuffer> LocalGpuMemoryBufferManager::ImportDmaBuf(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  if (handle.planes.size() !=
      gfx::NumberOfPlanesForLinearBufferFormat(format)) {
    // This could happen if e.g., we get a compressed RGBA buffer where one
    // plane is for metadata. We don't support this case.
    LOG(ERROR) << "Cannot import " << gfx::BufferFormatToString(format)
               << " with " << handle.planes.size() << " plane(s) (expected "
               << gfx::NumberOfPlanesForLinearBufferFormat(format)
               << " plane(s))";
    return nullptr;
  }
  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format) {
    LOG(ERROR) << "Unsupported format " << gfx::BufferFormatToString(format);
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
  return std::make_unique<GpuMemoryBufferImplGbm>(format, buffer_object);
}

bool LocalGpuMemoryBufferManager::IsFormatAndUsageSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  const uint32_t drm_format = GetDrmFormat(format);
  if (!drm_format)
    return false;
  const uint32_t gbm_usage = GetGbmUsage(usage);
  if (gbm_usage == 0)
    return false;
  return gbm_device_is_format_supported(gbm_device_.get(), drm_format,
                                        gbm_usage);
}

}  // namespace media
