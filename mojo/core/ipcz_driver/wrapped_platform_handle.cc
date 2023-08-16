// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"

#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/fdio/fd.h>
#include <lib/zx/handle.h>

#include "base/fuchsia/fuchsia_logging.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include <mach/mach.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#endif

namespace mojo::core::ipcz_driver {

namespace {

// Enumeration for different possible types of PlatformHandles that may be
// wrapped by a WrappedPlatformHandle.
enum class WrapperType : uint32_t {
  // The wrapped PlatformHandle is transmissible as-is and can pass through a
  // Transport unmodified.
  kTransmissible,

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
  // The wrapped PlatformHandle is a file descriptor on Fuchsia or macOS/iOS,
  // where the Transport can only transmit Fuchsia handles or Mach port rights,
  // respectively.
  //
  // On Fuchsia we serialize this type of handle by replacing it with a handle
  // to a corresponding fdio object. On macOS/iOS we replace it with a handle to
  // a corresponding fileport send right. On the receiving end this deserializes
  // back to a file descriptor.
  kIndirectFileDescriptor,
#endif
};

// Header for a serialized WrappedPlatformHandle object.
struct IPCZ_ALIGN(8) WrappedPlatformHandleHeader {
  // The size of this structure in bytes.
  uint32_t size;

  // Indicates what specific type of handle is wrapped.
  WrapperType type;
};
static_assert(sizeof(WrappedPlatformHandleHeader) == 8,
              "Invalid WrappedPlatformHandleHeader size");

#if BUILDFLAG(IS_FUCHSIA)
PlatformHandle MakeFDTransmissible(base::ScopedFD fd) {
  zx::handle result;
  const zx_status_t status =
      fdio_fd_transfer_or_clone(fd.release(), result.reset_and_get_address());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_transfer_or_clone";
    return {};
  }
  return PlatformHandle(std::move(result));
}

base::ScopedFD RecoverFDFromTransmissible(PlatformHandle handle) {
  base::ScopedFD fd;
  zx_status_t status = fdio_fd_create(handle.ReleaseHandle(),
                                      base::ScopedFD::Receiver(fd).get());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_create";
    return {};
  }
  return fd;
}
#elif BUILDFLAG(IS_APPLE)
extern "C" {
kern_return_t fileport_makeport(int fd, mach_port_t*);
int fileport_makefd(mach_port_t);
}  // extern "C"

PlatformHandle MakeFDTransmissible(base::ScopedFD fd) {
  base::apple::ScopedMachSendRight port;
  kern_return_t kr = fileport_makeport(
      fd.get(), base::apple::ScopedMachSendRight::Receiver(port).get());
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "fileport_makeport";
    return {};
  }
  return PlatformHandle(std::move(port));
}

base::ScopedFD RecoverFDFromTransmissible(PlatformHandle handle) {
  if (!handle.is_mach_send()) {
    return {};
  }
  return base::ScopedFD(fileport_makefd(handle.GetMachSendRight().get()));
}
#endif

}  // namespace

WrappedPlatformHandle::WrappedPlatformHandle() = default;

WrappedPlatformHandle::WrappedPlatformHandle(PlatformHandle handle)
    : handle_(std::move(handle)) {}

WrappedPlatformHandle::~WrappedPlatformHandle() = default;

void WrappedPlatformHandle::Close() {
  handle_.reset();
}

bool WrappedPlatformHandle::IsSerializable() const {
  return true;
}

bool WrappedPlatformHandle::GetSerializedDimensions(Transport& transmitter,
                                                    size_t& num_bytes,
                                                    size_t& num_handles) {
  DCHECK(handle_.is_valid());
  num_bytes = sizeof(WrappedPlatformHandleHeader);
  num_handles = 1;
  return true;
}

bool WrappedPlatformHandle::Serialize(Transport& transmitter,
                                      base::span<uint8_t> data,
                                      base::span<PlatformHandle> handles) {
  DCHECK(handle_.is_valid());
  DCHECK_EQ(sizeof(WrappedPlatformHandleHeader), data.size());
  DCHECK_EQ(1u, handles.size());

  PlatformHandle handle = std::move(handle_);
  WrapperType type = WrapperType::kTransmissible;

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
  if (handle.is_fd()) {
    type = WrapperType::kIndirectFileDescriptor;
    handle = MakeFDTransmissible(handle.TakeFD());
  }
#endif

  auto& header = *reinterpret_cast<WrappedPlatformHandleHeader*>(data.data());
  header.size = sizeof(header);
  header.type = type;
  handles[0] = std::move(handle);
  return true;
}

// static
scoped_refptr<WrappedPlatformHandle> WrappedPlatformHandle::Deserialize(
    base::span<const uint8_t> data,
    base::span<PlatformHandle> handles) {
  if (data.size() < sizeof(WrappedPlatformHandleHeader) ||
      handles.size() != 1) {
    return nullptr;
  }

  const auto& header =
      *reinterpret_cast<const WrappedPlatformHandleHeader*>(data.data());
  const size_t header_size = header.size;
  if (header_size < sizeof(header) || header_size % 8 != 0) {
    return nullptr;
  }

  PlatformHandle handle = std::move(handles[0]);
  switch (header.type) {
    case WrapperType::kTransmissible:
      break;

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
    case WrapperType::kIndirectFileDescriptor:
      handle = PlatformHandle(RecoverFDFromTransmissible(std::move(handle)));
      break;
#endif

    default:
      return nullptr;
  }

  if (!handle.is_valid()) {
    return nullptr;
  }

  return base::MakeRefCounted<WrappedPlatformHandle>(std::move(handle));
}

}  // namespace mojo::core::ipcz_driver
