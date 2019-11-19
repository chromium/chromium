// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of TegraV4L2Device used on
// Tegra platform.

#ifndef MEDIA_GPU_V4L2_TEGRA_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_TEGRA_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "ui/gl/gl_bindings.h"

namespace media {

// This class implements the V4L2Device interface for Tegra platform.
// It interfaces with libtegrav4l2 library which provides API that exhibit the
// V4L2 specification via the library API instead of system calls.
class TegraV4L2Device : public V4L2Device {
 public:
  TegraV4L2Device();

  // V4L2Device implementation.
  bool Open(Type type, uint32_t v4l2_pixfmt) override;
  int Ioctl(int flags, void* arg) override;
  bool Poll(bool poll_device, bool* event_pending) override;
  bool SetDevicePollInterrupt() override;
  bool ClearDevicePollInterrupt() override;
  void* Mmap(void* addr,
             unsigned int len,
             int prot,
             int flags,
             unsigned int offset) override;
  void Munmap(void* addr, unsigned int len) override;
  std::vector<base::ScopedFD> GetDmabufsForV4L2Buffer(
      int index,
      size_t num_planes,
      enum v4l2_buf_type buf_type) override;
  bool CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) override;
  EGLImageKHR CreateEGLImage(
      EGLDisplay egl_display,
      EGLContext egl_context,
      GLuint texture_id,
      const gfx::Size& size,
      unsigned int buffer_index,
      uint32_t v4l2_pixfmt,
      const std::vector<base::ScopedFD>& dmabuf_fds) override;
  scoped_refptr<gl::GLImage> CreateGLImage(
      const gfx::Size& size,
      uint32_t fourcc,
      const std::vector<base::ScopedFD>& dmabuf_fds) override;
  EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                             EGLImageKHR egl_image) override;
  GLenum GetTextureTarget() override;
  std::vector<uint32_t> PreferredInputFormat(Type type) override;

  std::vector<uint32_t> GetSupportedImageProcessorPixelformats(
      v4l2_buf_type buf_type) override;

  VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
      const size_t num_formats,
      const uint32_t pixelformats[]) override;

  VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles()
      override;

  bool IsImageProcessingSupported() override;

  bool IsJpegDecodingSupported() override;
  bool IsJpegEncodingSupported() override;

 private:
  ~TegraV4L2Device() override;

  bool Initialize() override;

  bool OpenInternal(Type type);
  void Close();

  // The actual device fd.
  int device_fd_ = -1;

  // The v4l2_format cache passed to the driver via VIDIOC_S_FMT. The key is
  // v4l2_buf_type.
  std::map<enum v4l2_buf_type, struct v4l2_format> v4l2_format_cache_;

  DISALLOW_COPY_AND_ASSIGN(TegraV4L2Device);
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_TEGRA_V4L2_DEVICE_H_
