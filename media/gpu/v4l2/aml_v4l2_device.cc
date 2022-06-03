// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include <errno.h>
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/aml_v4l2_device.h"
#include "ui/gl/gl_bindings.h"

namespace media {

namespace {
const char kEncoderDevice[] = "/dev/amvenc_avc";
}
void* libvpcodec;

typedef void* (*AmlV4L2Create)();
typedef void (*AmlV4L2Destroy)(void* context);
typedef int32_t (*AmlV4L2Open)(void* context, const char* name);
typedef int32_t (*AmlV4L2Close)(void* context, int32_t fd);
typedef int32_t (*AmlV4L2Ioctl)(void* context, int32_t fd, int cmd, ...);
typedef int32_t (*AmlV4L2Poll)(void* context,
                               int32_t fd,
                               bool poll_device,
                               bool* event_pending);
typedef int32_t (*AmlV4L2SetDevicePollInterrupt)(void* context, int32_t fd);
typedef int32_t (*AmlV4L2ClearDevicePollInterrupt)(void* context, int32_t fd);
typedef void* (*AmlV4L2Mmap)(void* context,
                             void* addr,
                             size_t length,
                             int prot,
                             int flags,
                             int fd,
                             unsigned int offset);
typedef int32_t (*AmlV4L2Munmap)(void* context, void* addr, size_t length);

#define AMLV4L2_SYM(name) AmlV4L2##name AmlV4L2_##name = nullptr

AMLV4L2_SYM(Create);
AMLV4L2_SYM(Destroy);
AMLV4L2_SYM(Open);
AMLV4L2_SYM(Close);
AMLV4L2_SYM(Ioctl);
AMLV4L2_SYM(Poll);
AMLV4L2_SYM(SetDevicePollInterrupt);
AMLV4L2_SYM(ClearDevicePollInterrupt);
AMLV4L2_SYM(Mmap);
AMLV4L2_SYM(Munmap);

#undef AMLV4L2_SYM

AmlV4L2Device::AmlV4L2Device() {}

AmlV4L2Device::~AmlV4L2Device() {
  if (context_) {
    CloseDevice();
    AmlV4L2_Destroy(context_);
  }
}

int AmlV4L2Device::Ioctl(int flags, void* arg) {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::Ioctl(flags, arg);

  return HANDLE_EINTR(AmlV4L2_Ioctl(context_, device_fd_.get(),
                                    static_cast<unsigned long>(flags), arg));
}

bool AmlV4L2Device::Poll(bool poll_device, bool* event_pending) {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::Poll(poll_device, event_pending);

  if (HANDLE_EINTR(AmlV4L2_Poll(context_, device_fd_.get(), poll_device,
                                event_pending)) == -1) {
    VLOGF(1) << "AmlV4L2Poll returned -1 ";
    return false;
  }
  return true;
}

void* AmlV4L2Device::Mmap(void* addr,
                          unsigned int len,
                          int prot,
                          int flags,
                          unsigned int offset) {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::Mmap(addr, len, prot, flags, offset);

  return AmlV4L2_Mmap(context_, addr, len, prot, flags, device_fd_.get(),
                      offset);
}

void AmlV4L2Device::Munmap(void* addr, unsigned int len) {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::Munmap(addr, len);

  AmlV4L2_Munmap(context_, addr, len);
}

bool AmlV4L2Device::SetDevicePollInterrupt() {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::SetDevicePollInterrupt();

  if (HANDLE_EINTR(
          AmlV4L2_SetDevicePollInterrupt(context_, device_fd_.get())) == -1) {
    VLOGF(1) << "Error in calling AmlV4L2SetDevicePollInterrupt";
    return false;
  }
  return true;
}

bool AmlV4L2Device::ClearDevicePollInterrupt() {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::ClearDevicePollInterrupt();

  if (HANDLE_EINTR(
          AmlV4L2_ClearDevicePollInterrupt(context_, device_fd_.get())) == -1) {
    VLOGF(1) << "Error in calling AmlV4L2ClearDevicePollInterrupt";
    return false;
  }
  return true;
}

bool AmlV4L2Device::Initialize() {
  if (!GenericV4L2Device::Initialize())
    return false;

  static bool initialized = []() {
    libvpcodec = dlopen("/system/lib/libvpcodec.so",
                        RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE | RTLD_LAZY);
    if (libvpcodec == nullptr) {
      VLOGF(1) << "Failed to dlsym load libvpcodec.so";
      return false;
    }

#define AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(lib, name)                    \
  do {                                                                 \
    AmlV4L2_##name =                                                   \
        reinterpret_cast<AmlV4L2##name>(dlsym(lib, "AmlV4L2_" #name)); \
    if (AmlV4L2_##name == nullptr) {                                   \
      VLOGF(1) << "Failed to dlsym AmlV4L2_" #name;                    \
      VLOGF(1) << "Failed to dlsym AmlV4L2_ == " << strerror(errno);   \
      VLOGF(1) << "Failed to dlerror  == " << dlerror();               \
      return false;                                                    \
    }                                                                  \
  } while (0)

    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Create);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Destroy);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Open);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Close);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Ioctl);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Poll);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, SetDevicePollInterrupt);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, ClearDevicePollInterrupt);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Mmap);
    AMLV4L2_DLSYM_OR_RETURN_ON_ERROR(libvpcodec, Munmap);
#undef AMLV4L2_DLSYM_OR_RETURN_ON_ERROR
    return true;
  }();

  if (!initialized) {
    LOG(ERROR) << "Failed to load AmlV4L2 symbols";
    return false;
  }

  context_ = AmlV4L2_Create();
  if (!context_) {
    LOG(ERROR) << "Fail to create context";
    return false;
  }

  return initialized;
}

bool AmlV4L2Device::Open(Type type, uint32_t v4l2_pixfmt) {
  type_ = type;
  if (type != Type::kEncoder)
    return GenericV4L2Device::Open(type, v4l2_pixfmt);

  return OpenDevice();
}

std::vector<uint32_t> AmlV4L2Device::PreferredInputFormat(Type type) const {
  if (type_ != Type::kEncoder)
    return GenericV4L2Device::PreferredInputFormat(type);

  return {V4L2_PIX_FMT_YUV420M};
}

VideoEncodeAccelerator::SupportedProfiles
AmlV4L2Device::GetSupportedEncodeProfiles() {
  if (!OpenDevice())
    return VideoEncodeAccelerator::SupportedProfiles();
  type_ = Type::kEncoder;
  const auto& profiles = EnumerateSupportedEncodeProfiles();

  CloseDevice();
  return profiles;
}

bool AmlV4L2Device::OpenDevice() {
  DCHECK(!device_fd_.is_valid());
  device_fd_.reset(HANDLE_EINTR(AmlV4L2_Open(context_, kEncoderDevice)));
  if (!device_fd_.is_valid()) {
    LOG(ERROR) << "Unable to open device " << kEncoderDevice;
    return false;
  }
  return true;
}

void AmlV4L2Device::CloseDevice() {
  // Release the fd as AMLV4L2_Close closes the fd.
  if (device_fd_.is_valid()) {
    AmlV4L2_Close(context_, device_fd_.release());
  }
}

}  //  namespace media
