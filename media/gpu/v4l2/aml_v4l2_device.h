// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of AmlV4L2Device used on
// Aml platform.

#ifndef MEDIA_GPU_V4L2_AML_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_AML_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_file.h"
#include "media/gpu/v4l2/generic_v4l2_device.h"
#include "ui/gl/gl_bindings.h"

namespace media {

// This class implements the V4L2Device interface for Aml platform.
// It interfaces with libamlv4l2 library which provides API that exhibit the
// V4L2 specification via the library API instead of system calls. libvpcodec
// only supports encoder, all other type of requests will go through
// GenericV4L2Device.
class AmlV4L2Device : public GenericV4L2Device {
 public:
  AmlV4L2Device();

  AmlV4L2Device(const AmlV4L2Device&) = delete;
  AmlV4L2Device& operator=(const AmlV4L2Device&) = delete;

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
  std::vector<uint32_t> PreferredInputFormat(Type type) const override;
  VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles()
      override;

 private:
  ~AmlV4L2Device() override;

  bool Initialize() override;

  bool OpenDevice();
  void CloseDevice();

  // The actual device fd.
  base::ScopedFD device_fd_;

  void* context_ = nullptr;

  Type type_ = Type::kEncoder;
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_AML_V4L2_DEVICE_H_
