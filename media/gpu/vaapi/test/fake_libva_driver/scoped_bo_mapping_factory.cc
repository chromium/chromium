// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/scoped_bo_mapping_factory.h"

#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"

namespace media::internal {

ScopedBOMapping::ScopedAccess::ScopedAccess(const ScopedBOMapping& mapping)
    : mapping_(mapping) {
  for (const auto& plane : mapping_->planes_) {
    struct dma_buf_sync sync_start;
    memset(&sync_start, 0, sizeof(sync_start));
    sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    PCHECK(HANDLE_EINTR(ioctl(plane.prime_fd.get(), DMA_BUF_IOCTL_SYNC,
                              &sync_start)) == 0);
  }
}

ScopedBOMapping::ScopedAccess::~ScopedAccess() {
  for (const auto& plane : mapping_->planes_) {
    struct dma_buf_sync sync_end;
    memset(&sync_end, 0, sizeof(sync_end));
    sync_end.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    PCHECK(HANDLE_EINTR(ioctl(plane.prime_fd.get(), DMA_BUF_IOCTL_SYNC,
                              &sync_end)) == 0);
  }
}

uint8_t* ScopedBOMapping::ScopedAccess::GetData(size_t plane) const {
  CHECK_LT(plane, mapping_->planes_.size());
  return static_cast<uint8_t*>(mapping_->planes_[plane].addr);
}

uint32_t ScopedBOMapping::ScopedAccess::GetStride(size_t plane) const {
  CHECK_LT(plane, mapping_->planes_.size());
  return mapping_->planes_[plane].stride;
}

ScopedBOMapping::ScopedBOMapping()
    : scoped_bo_mapping_factory_(nullptr), bo_import_(nullptr) {}

ScopedBOMapping::ScopedBOMapping(
    ScopedBOMappingFactory* scoped_bo_mapping_factory,
    std::vector<Plane> planes,
    struct gbm_bo* bo_import)
    : scoped_bo_mapping_factory_(scoped_bo_mapping_factory),
      planes_(std::move(planes)),
      bo_import_(bo_import) {}

ScopedBOMapping::ScopedBOMapping(ScopedBOMapping&& other)
    : scoped_bo_mapping_factory_(std::move(other.scoped_bo_mapping_factory_)),
      planes_(std::move(other.planes_)),
      bo_import_(std::move(other.bo_import_)) {
  // Note: we explicitly set these members to nullptr because a raw_ptr<T> may
  // or may not be zeroed out on move (it depends on the build configuration).
  other.scoped_bo_mapping_factory_ = nullptr;
  other.bo_import_ = nullptr;
}

ScopedBOMapping& ScopedBOMapping::operator=(ScopedBOMapping&& other) {
  // Note: we explicitly set |other.scoped_bo_mapping_factory_| and
  // |other.bo_import_| to nullptr because a raw_ptr<T> may or may not be zeroed
  // out on move (it depends on the build configuration).
  scoped_bo_mapping_factory_ = std::move(other.scoped_bo_mapping_factory_);
  other.scoped_bo_mapping_factory_ = nullptr;

  planes_ = std::move(other.planes_);

  bo_import_ = std::move(other.bo_import_);
  other.bo_import_ = nullptr;

  return *this;
}

ScopedBOMapping::~ScopedBOMapping() {
  if (IsValid()) {
    // We tell the factory to unmap and destroy the Buffer Object as it
    // does so under a lock which cannot be done here.
    scoped_bo_mapping_factory_->UnmapAndDestroyBufferObject(std::move(planes_),
                                                            bo_import_);
  }
}

ScopedBOMapping::Plane::Plane(uint32_t stride,
                              void* addr,
                              void* mmap_data,
                              int prime_fd)
    : stride(stride), addr(addr), mmap_data(mmap_data), prime_fd(prime_fd) {}

ScopedBOMapping::Plane::Plane(Plane&& other)
    : stride(other.stride),
      addr(std::move(other.addr)),
      mmap_data(std::move(other.mmap_data)),
      prime_fd(std::move(other.prime_fd)) {
  other.stride = 0u;

  // Note: we explicitly set these members to nullptr because a raw_ptr<T> may
  // or may not be zeroed out on move (it depends on the build configuration).
  other.addr = nullptr;
  other.mmap_data = nullptr;
}

ScopedBOMapping::Plane& ScopedBOMapping::Plane::operator=(
    ScopedBOMapping::Plane&& other) {
  stride = other.stride;
  other.stride = 0u;

  // Note: we explicitly set |other.addr| and |other.mmap_data| to nullptr
  // because a raw_ptr<T> may or may not be zeroed out on move (it depends on
  // the build configuration).
  addr = std::move(other.addr);
  other.addr = nullptr;

  mmap_data = std::move(other.mmap_data);
  other.mmap_data = nullptr;

  prime_fd = std::move(other.prime_fd);

  return *this;
}

ScopedBOMapping::Plane::~Plane() = default;

ScopedBOMapping::ScopedAccess ScopedBOMapping::BeginAccess() const {
  CHECK(IsValid());
  return ScopedBOMapping::ScopedAccess(*this);
}

ScopedBOMappingFactory::ScopedBOMappingFactory(int drm_fd)
    : gbm_device_(gbm_create_device(drm_fd)) {
  CHECK_GE(drm_fd, 0);
  CHECK(gbm_device_);
}

ScopedBOMappingFactory::~ScopedBOMappingFactory() = default;

ScopedBOMapping ScopedBOMappingFactory::Create(
    gbm_import_fd_modifier_data import_data) {
#if defined(MINIGBM)
  base::AutoLock lock(lock_);
  struct gbm_bo* bo_import =
      gbm_bo_import(gbm_device_.get(), GBM_BO_IMPORT_FD_MODIFIER, &import_data,
                    GBM_BO_USE_SW_READ_OFTEN | GBM_BO_USE_SW_WRITE_OFTEN);
  CHECK(bo_import);

  std::vector<ScopedBOMapping::Plane> planes;
  for (int plane = 0; plane < gbm_bo_get_plane_count(bo_import); plane++) {
    uint32_t stride;
    void* mmap_data;
    void* addr =
        gbm_bo_map2(bo_import, /*x=*/0, /*y=*/0, gbm_bo_get_width(bo_import),
                    gbm_bo_get_height(bo_import), GBM_BO_TRANSFER_READ_WRITE,
                    &stride, &mmap_data, plane);
    CHECK_NE(addr, MAP_FAILED);
    CHECK_GE(stride, 0u);
    CHECK(mmap_data);

    const int prime_fd = gbm_bo_get_fd_for_plane(bo_import, plane);
    CHECK_GE(prime_fd, 0);

    planes.emplace_back(stride, addr, mmap_data, prime_fd);
  }
  return ScopedBOMapping(this, std::move(planes), bo_import);
#else
  NOTIMPLEMENTED();
  return ScopedBOMapping();
#endif
}

void ScopedBOMappingFactory::UnmapAndDestroyBufferObject(
    std::vector<ScopedBOMapping::Plane> planes,
    struct gbm_bo* bo_import) {
  base::AutoLock lock(lock_);
  CHECK(bo_import);
  CHECK(gbm_device_);
  for (const auto& plane : planes) {
    CHECK(plane.mmap_data);
    gbm_bo_unmap(bo_import, plane.mmap_data);
  }

  // Note that calling gbm_bo_destroy() may not actually end up destroying the
  // buffer object. That's because minigbm is expected to do reference counting
  // of GEM handles. This is fine: suppose we create multiple ScopedBOMapping
  // instances for the same dma-buf; all of them should share the same GEM
  // handle which should only get destroyed when all the ScopedBOMappings
  // sharing that GEM handle are destroyed.
  gbm_bo_destroy(bo_import);
}

}  // namespace media::internal
