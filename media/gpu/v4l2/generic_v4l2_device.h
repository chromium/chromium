// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of GenericV4L2Device used on
// platforms, which provide generic V4L2 video codec devices.

#ifndef MEDIA_GPU_V4L2_GENERIC_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_GENERIC_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/files/scoped_file.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

class GenericV4L2Device : public V4L2Device {
 public:
  GenericV4L2Device();

  GenericV4L2Device(const GenericV4L2Device&) = delete;
  GenericV4L2Device& operator=(const GenericV4L2Device&) = delete;

  // V4L2Device implementation.
  bool Open(Type type, uint32_t v4l2_pixfmt) override;
  int Ioctl(int request, void* arg) override;
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

  bool CanCreateEGLImageFrom(const Fourcc fourcc) const override;
  EGLImageKHR CreateEGLImage(EGLDisplay egl_display,
                             EGLContext egl_context,
                             GLuint texture_id,
                             const gfx::Size& size,
                             unsigned int buffer_index,
                             const Fourcc fourcc,
                             gfx::NativePixmapHandle handle) const override;

  EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                             EGLImageKHR egl_image) const override;
  GLenum GetTextureTarget() const override;
  std::vector<uint32_t> PreferredInputFormat(Type type) const override;

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

 protected:
  ~GenericV4L2Device() override;

  bool Initialize() override;

 private:
  // Vector of video device node paths and corresponding pixelformats supported
  // by each device node.
  using Devices = std::vector<std::pair<std::string, std::vector<uint32_t>>>;

  // Open device node for |path| as a device of |type|.
  bool OpenDevicePath(const std::string& path, Type type);

  // Close the currently open device.
  void CloseDevice();

  // Enumerate all V4L2 devices on the system for |type| and store the results
  // under devices_by_type_[type].
  void EnumerateDevicesForType(V4L2Device::Type type);

  // Return device information for all devices of |type| available in the
  // system. Enumerates and queries devices on first run and caches the results
  // for subsequent calls.
  const Devices& GetDevicesForType(V4L2Device::Type type);

  // Return device node path for device of |type| supporting |pixfmt|, or
  // an empty string if the given combination is not supported by the system.
  std::string GetDevicePathFor(V4L2Device::Type type, uint32_t pixfmt);

  // Stores information for all devices available on the system
  // for each device Type.
  std::map<V4L2Device::Type, Devices> devices_by_type_;

  // The actual device fd.
  base::ScopedFD device_fd_;

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  base::ScopedFD device_poll_interrupt_fd_;

  // Use libv4l2 when operating |device_fd_|.
  bool use_libv4l2_;

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();
};
}  //  namespace media

#endif  // MEDIA_GPU_V4L2_GENERIC_V4L2_DEVICE_H_
