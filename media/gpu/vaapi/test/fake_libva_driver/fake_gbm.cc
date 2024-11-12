// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// We need to conform to the GBM API, which unfortunately involves a lot of
// unsafe buffer access to maintain C99 compatibility.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/fake_libva_driver/fake_gbm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"

#define PAGE_SIZE 4096
#define ALIGN(x, y) (((x + y - 1) / y) * y)

struct gbm_bo {
  struct gbm_import_fd_modifier_data meta;
};

struct gbm_bo_mapping {
  raw_ptr<void> addr;
  size_t size;
};

uint32_t get_y_subsample(struct gbm_bo* bo, size_t plane) {
  if (plane == 0) {
    return 1;
  } else {
    return 2;
  }
}

uint32_t get_x_subsample(struct gbm_bo* bo, size_t plane) {
  if (plane == 0) {
    return 1;
  }

  switch (bo->meta.format) {
    case GBM_FORMAT_NV12:
    case GBM_FORMAT_P010:
      return 1;
    case GBM_FORMAT_YUV420:
      return 2;
    default:
      NOTREACHED();
  }
}

uint32_t get_plane_min_size(struct gbm_bo* bo, size_t plane) {
  return bo->meta.strides[plane] *
         ALIGN(bo->meta.height, get_y_subsample(bo, plane)) /
         get_y_subsample(bo, plane);
}

extern "C" GBM_EXPORT struct gbm_device* gbm_create_device(int fd) {
  return new struct gbm_device;
}

extern "C" GBM_EXPORT void gbm_device_destroy(struct gbm_device* gbm) {
  delete gbm;
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_bpp(struct gbm_bo* bo) {
  CHECK(bo);

  switch (bo->meta.format) {
    case GBM_FORMAT_NV12:
    case GBM_FORMAT_YUV420:
      return 1;
    case GBM_FORMAT_P010:
      return 2;
    default:
      NOTREACHED();
  }
}

extern "C" GBM_EXPORT int gbm_bo_get_plane_count(struct gbm_bo* bo) {
  CHECK(bo);

  switch (bo->meta.format) {
    case GBM_FORMAT_NV12:
    case GBM_FORMAT_P010:
      return 2;
    case GBM_FORMAT_YUV420:
      return 3;
    default:
      NOTREACHED();
  }
}

extern "C" GBM_EXPORT struct gbm_bo* gbm_bo_create(struct gbm_device* gbm,
                                                   uint32_t width,
                                                   uint32_t height,
                                                   uint32_t format,
                                                   uint32_t flags) {
  CHECK(format == GBM_FORMAT_NV12 || format == GBM_FORMAT_P010 ||
        format == GBM_FORMAT_YUV420);

  FILE* backing_file = tmpfile();
  CHECK(backing_file);
  int fd = fileno(backing_file);

  struct gbm_bo* bo = new struct gbm_bo;
  bo->meta.num_fds = 1;
  bo->meta.fds[0] = fd;
  bo->meta.width = width;
  bo->meta.height = height;
  bo->meta.format = format;
  bo->meta.modifier = 0;

  uint32_t size = 0;
  for (int i = 0; i < gbm_bo_get_plane_count(bo); i++) {
    bo->meta.offsets[i] = size;
    bo->meta.strides[i] = ALIGN(width, get_x_subsample(bo, i)) /
                          get_x_subsample(bo, i) * gbm_bo_get_bpp(bo);
    size += ALIGN(get_plane_min_size(bo, i), PAGE_SIZE);
  }

  CHECK(ftruncate(fd, size) == 0);

  return bo;
}

extern "C" GBM_EXPORT void gbm_bo_destroy(struct gbm_bo* bo) {
  CHECK(bo);
  delete bo;
}

extern "C" GBM_EXPORT void* gbm_bo_map2(struct gbm_bo* bo,
                                        uint32_t x,
                                        uint32_t y,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t transfer_flags,
                                        uint32_t* stride,
                                        void** map_data,
                                        int plane) {
  CHECK(bo);
  CHECK(x + width <= bo->meta.width);
  CHECK(y + height <= bo->meta.height);
  CHECK(ALIGN(x, get_x_subsample(bo, plane)) == x);
  CHECK(ALIGN(y, get_y_subsample(bo, plane)) == y);
  CHECK(static_cast<int>(plane) < gbm_bo_get_plane_count(bo));

  size_t size = ALIGN(get_plane_min_size(bo, plane), PAGE_SIZE);
  void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    bo->meta.fds[0], bo->meta.offsets[plane]);
  CHECK(addr);
  CHECK(addr != MAP_FAILED);

  struct gbm_bo_mapping** _map_data =
      reinterpret_cast<struct gbm_bo_mapping**>(map_data);
  *_map_data = new struct gbm_bo_mapping;
  (*_map_data)->addr = addr;
  (*_map_data)->size = size;
  *stride = bo->meta.strides[plane];

  size_t offset = y / get_y_subsample(bo, plane) * bo->meta.strides[plane] +
                  x / get_x_subsample(bo, plane) * gbm_bo_get_bpp(bo);
  return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(addr) + offset);
}

extern "C" GBM_EXPORT void gbm_bo_unmap(struct gbm_bo* bo, void* map_data) {
  CHECK(bo);

  struct gbm_bo_mapping* _map_data =
      reinterpret_cast<struct gbm_bo_mapping*>(map_data);
  void* addr = _map_data->addr;
  size_t size = _map_data->size;
  munmap(addr, size);
  delete _map_data;
}

extern "C" GBM_EXPORT struct gbm_bo* gbm_bo_import(struct gbm_device* gbm,
                                                   uint32_t type,
                                                   void* buffer,
                                                   uint32_t usage) {
  CHECK(buffer);
  CHECK(type == GBM_BO_IMPORT_FD_MODIFIER || type == GBM_BO_IMPORT_FD_PLANAR);

  struct gbm_bo* bo = new struct gbm_bo;
  bo->meta = *reinterpret_cast<struct gbm_import_fd_modifier_data*>(buffer);

  return bo;
}

extern "C" GBM_EXPORT int gbm_bo_get_fd(struct gbm_bo* bo) {
  CHECK(bo);

  return dup(bo->meta.fds[0]);
}

extern "C" GBM_EXPORT int gbm_bo_get_fd_for_plane(struct gbm_bo* bo,
                                                  size_t plane) {
  CHECK(bo);
  CHECK(static_cast<int>(plane) < gbm_bo_get_plane_count(bo));

  return gbm_bo_get_fd(bo);
}

extern "C" GBM_EXPORT uint64_t gbm_bo_get_modifier(struct gbm_bo* bo) {
  return 0;
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_format(struct gbm_bo* bo) {
  CHECK(bo);

  return bo->meta.format;
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_offset(struct gbm_bo* bo,
                                                 size_t plane) {
  CHECK(bo);
  CHECK(static_cast<int>(plane) < gbm_bo_get_plane_count(bo));

  return bo->meta.offsets[plane];
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_stride_for_plane(struct gbm_bo* bo,
                                                           size_t plane) {
  CHECK(bo);
  CHECK(static_cast<int>(plane) < gbm_bo_get_plane_count(bo));

  return bo->meta.strides[plane];
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_width(struct gbm_bo* bo) {
  CHECK(bo);

  return bo->meta.width;
}

extern "C" GBM_EXPORT uint32_t gbm_bo_get_height(struct gbm_bo* bo) {
  CHECK(bo);

  return bo->meta.height;
}
