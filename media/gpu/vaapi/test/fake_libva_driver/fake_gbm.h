// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_GBM_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_GBM_H_

#include <stdint.h>

#include <memory>

#define GBM_EXPORT __attribute__((visibility("default")))

#define FAKE_GBM_FOURCC(a, b, c, d)                                       \
  ((((uint32_t)d) << 24) | (((uint32_t)c) << 16) | (((uint32_t)b) << 8) | \
   ((uint32_t)a))

#define GBM_FORMAT_NV12 FAKE_GBM_FOURCC('N', 'V', '1', '2')
#define GBM_FORMAT_P010 FAKE_GBM_FOURCC('P', '0', '1', '0')
#define GBM_FORMAT_YUV420 FAKE_GBM_FOURCC('Y', 'U', '1', '2')

#define GBM_BO_IMPORT_FD_PLANAR 0x5504
#define GBM_BO_IMPORT_FD_MODIFIER 0x5505

#define GBM_MAX_PLANES 3

struct gbm_bo;

struct gbm_device {
  uint8_t pad;
};

struct gbm_import_fd_modifier_data {
  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t num_fds;
  int fds[GBM_MAX_PLANES];
  int strides[GBM_MAX_PLANES];
  int offsets[GBM_MAX_PLANES];
  uint64_t modifier;
};

extern "C" GBM_EXPORT struct gbm_device* gbm_create_device(int fd);
extern "C" GBM_EXPORT void gbm_device_destroy(struct gbm_device* gbm);
extern "C" GBM_EXPORT struct gbm_bo* gbm_bo_create(struct gbm_device* gbm,
                                                   uint32_t width,
                                                   uint32_t height,
                                                   uint32_t format,
                                                   uint32_t flags);
extern "C" GBM_EXPORT void gbm_bo_destroy(struct gbm_bo* bo);
extern "C" GBM_EXPORT void* gbm_bo_map2(struct gbm_bo* bo,
                                        uint32_t x,
                                        uint32_t y,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t transfer_flags,
                                        uint32_t* stride,
                                        void** map_data,
                                        int plane);
extern "C" GBM_EXPORT void gbm_bo_unmap(struct gbm_bo* bo, void* map_data);
extern "C" GBM_EXPORT struct gbm_bo* gbm_bo_import(struct gbm_device* gbm,
                                                   uint32_t type,
                                                   void* buffer,
                                                   uint32_t usage);
extern "C" GBM_EXPORT int gbm_bo_get_fd(struct gbm_bo* bo);
extern "C" GBM_EXPORT int gbm_bo_get_fd_for_plane(struct gbm_bo* bo,
                                                  size_t plane);
extern "C" GBM_EXPORT uint64_t gbm_bo_get_modifier(struct gbm_bo* bo);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_format(struct gbm_bo* bo);
extern "C" GBM_EXPORT int gbm_bo_get_plane_count(struct gbm_bo* bo);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_offset(struct gbm_bo* bo,
                                                 size_t plane);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_stride_for_plane(struct gbm_bo* bo,
                                                           size_t plane);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_bpp(struct gbm_bo* bo);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_width(struct gbm_bo* bo);
extern "C" GBM_EXPORT uint32_t gbm_bo_get_height(struct gbm_bo* bo);

#define GBM_BO_USE_SW_READ_OFTEN (1 << 9)
#define GBM_BO_USE_SW_WRITE_OFTEN (1 << 11)

#define GBM_BO_TRANSFER_READ_WRITE 0b11

namespace ui {
using ScopedGbmDevice = std::unique_ptr<gbm_device>;
}

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_GBM_H_
