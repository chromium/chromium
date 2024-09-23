// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_handle.h"

#include <tuple>

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include <lib/fdio/limits.h>
#include <unistd.h>
#include <zircon/status.h>

#include "base/fuchsia/fuchsia_logging.h"
#elif BUILDFLAG(IS_APPLE)
#include <mach/vm_map.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>

#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/binder.h"
#endif

namespace mojo {

namespace {

#if BUILDFLAG(IS_WIN)
base::win::ScopedHandle CloneHandle(const base::win::ScopedHandle& handle) {
  DCHECK(handle.IsValid());

  HANDLE dupe;
  BOOL result = FALSE;

  // INVALID_HANDLE_VALUE and the process pseudo-handle are both represented as
  // the value -1. This means that if a caller does not correctly check the
  // handle returned by file and pipe creation APIs, then it would pass an
  // INVALID_HANDLE_VALUE to the code below, which would result in the
  // destination process getting full control over the calling process (see
  // http://crbug.com/243339 for an example of this vulnerability). So, we just
  // explicitly check for INVALID_HANDLE_VALUE, since there's no valid scenario
  // in which it would be passed as the source handle here.
  if (handle.Get() != INVALID_HANDLE_VALUE) {
    result = ::DuplicateHandle(::GetCurrentProcess(), handle.Get(),
                               ::GetCurrentProcess(), &dupe, 0, FALSE,
                               DUPLICATE_SAME_ACCESS);
  }
  if (!result)
    return base::win::ScopedHandle();
  DCHECK_NE(dupe, INVALID_HANDLE_VALUE);
  return base::win::ScopedHandle(dupe);
}
#elif BUILDFLAG(IS_FUCHSIA)
zx::handle CloneHandle(const zx::handle& handle) {
  DCHECK(handle.is_valid());

  zx::handle dupe;
  zx_status_t result = handle.duplicate(ZX_RIGHT_SAME_RIGHTS, &dupe);
  if (result != ZX_OK)
    ZX_DLOG(ERROR, result) << "zx_duplicate_handle";
  return std::move(dupe);
}
#elif BUILDFLAG(IS_APPLE)
base::apple::ScopedMachSendRight CloneMachPort(
    const base::apple::ScopedMachSendRight& mach_port) {
  DCHECK(mach_port.is_valid());

  kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port.get(),
                                        MACH_PORT_RIGHT_SEND, 1);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(ERROR, kr) << "mach_port_mod_refs";
    return base::apple::ScopedMachSendRight();
  }
  return base::apple::ScopedMachSendRight(mach_port.get());
}
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
base::ScopedFD CloneFD(const base::ScopedFD& fd) {
  DCHECK(fd.is_valid());
  return base::ScopedFD(dup(fd.get()));
}
#endif

}  // namespace

PlatformHandle::PlatformHandle() = default;

PlatformHandle::PlatformHandle(PlatformHandle&& other) {
  *this = std::move(other);
}

#if BUILDFLAG(IS_WIN)
PlatformHandle::PlatformHandle(base::win::ScopedHandle handle)
    : type_(Type::kHandle), handle_(std::move(handle)) {}
#elif BUILDFLAG(IS_FUCHSIA)
PlatformHandle::PlatformHandle(zx::handle handle)
    : type_(Type::kHandle), handle_(std::move(handle)) {}
#elif BUILDFLAG(IS_APPLE)
PlatformHandle::PlatformHandle(base::apple::ScopedMachSendRight mach_port)
    : type_(Type::kMachSend), mach_send_(std::move(mach_port)) {}
PlatformHandle::PlatformHandle(base::apple::ScopedMachReceiveRight mach_port)
    : type_(Type::kMachReceive), mach_receive_(std::move(mach_port)) {}
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
PlatformHandle::PlatformHandle(base::ScopedFD fd)
    : type_(Type::kFd), fd_(std::move(fd)) {
#if BUILDFLAG(IS_FUCHSIA)
  DCHECK_LT(fd_.get(), FDIO_MAX_FD);
#endif
}
#endif

#if BUILDFLAG(IS_ANDROID)
PlatformHandle::PlatformHandle(base::android::BinderRef binder)
    : type_(Type::kBinder), binder_(std::move(binder)) {}
#endif

PlatformHandle::~PlatformHandle() = default;

PlatformHandle& PlatformHandle::operator=(PlatformHandle&& other) {
  type_ = other.type_;
  other.type_ = Type::kNone;

#if BUILDFLAG(IS_WIN)
  handle_ = std::move(other.handle_);
#elif BUILDFLAG(IS_FUCHSIA)
  handle_ = std::move(other.handle_);
#elif BUILDFLAG(IS_APPLE)
  mach_send_ = std::move(other.mach_send_);
  mach_receive_ = std::move(other.mach_receive_);
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  fd_ = std::move(other.fd_);
#endif

#if BUILDFLAG(IS_ANDROID)
  binder_ = std::move(other.binder_);
#endif
  return *this;
}

// static
void PlatformHandle::ToMojoPlatformHandle(PlatformHandle handle,
                                          MojoPlatformHandle* out_handle) {
  DCHECK(out_handle);
  out_handle->struct_size = sizeof(MojoPlatformHandle);
  if (handle.type_ == Type::kNone) {
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
    out_handle->value = 0;
    return;
  }

  do {
#if BUILDFLAG(IS_WIN)
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
    out_handle->value =
        static_cast<uint64_t>(HandleToLong(handle.TakeHandle().Take()));
    break;
#elif BUILDFLAG(IS_FUCHSIA)
    if (handle.is_handle()) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;
      out_handle->value = handle.TakeHandle().release();
      break;
    }
#elif BUILDFLAG(IS_APPLE)
    if (handle.is_mach_send()) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_MACH_SEND_RIGHT;
      out_handle->value = static_cast<uint64_t>(handle.ReleaseMachSendRight());
      break;
    } else if (handle.is_mach_receive()) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_MACH_RECEIVE_RIGHT;
      out_handle->value =
          static_cast<uint64_t>(handle.ReleaseMachReceiveRight());
      break;
    }
#endif

#if BUILDFLAG(IS_ANDROID)
    if (handle.type_ == Type::kBinder) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_BINDER;
      out_handle->value = reinterpret_cast<uint64_t>(handle.binder_.release());
      return;
    }
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    DCHECK(handle.is_fd());
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
    out_handle->value = static_cast<uint64_t>(handle.TakeFD().release());
#endif
  } while (false);

  // One of the above cases must take ownership of |handle|.
  DCHECK(!handle.is_valid());
}

// static
PlatformHandle PlatformHandle::FromMojoPlatformHandle(
    const MojoPlatformHandle* handle) {
  if (handle->struct_size < sizeof(*handle) ||
      handle->type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    return PlatformHandle();
  }

#if BUILDFLAG(IS_WIN)
  if (handle->type != MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
    return PlatformHandle();
  return PlatformHandle(
      base::win::ScopedHandle(LongToHandle(static_cast<long>(handle->value))));
#elif BUILDFLAG(IS_FUCHSIA)
  if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
    return PlatformHandle(zx::handle(handle->value));
#elif BUILDFLAG(IS_APPLE)
  if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_MACH_SEND_RIGHT) {
    return PlatformHandle(base::apple::ScopedMachSendRight(
        static_cast<mach_port_t>(handle->value)));
  } else if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_MACH_RECEIVE_RIGHT) {
    return PlatformHandle(base::apple::ScopedMachReceiveRight(
        static_cast<mach_port_t>(handle->value)));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_BINDER) {
    return PlatformHandle(
        base::android::BinderRef(reinterpret_cast<AIBinder*>(handle->value)));
  }
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (handle->type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return PlatformHandle();
  return PlatformHandle(base::ScopedFD(static_cast<int>(handle->value)));
#endif
}

void PlatformHandle::reset() {
  type_ = Type::kNone;

#if BUILDFLAG(IS_WIN)
  handle_.Close();
#elif BUILDFLAG(IS_FUCHSIA)
  handle_.reset();
#elif BUILDFLAG(IS_APPLE)
  mach_send_.reset();
  mach_receive_.reset();
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  fd_.reset();
#endif

#if BUILDFLAG(IS_ANDROID)
  binder_.reset();
#endif
}

void PlatformHandle::release() {
  type_ = Type::kNone;

#if BUILDFLAG(IS_WIN)
  std::ignore = handle_.Take();
#elif BUILDFLAG(IS_FUCHSIA)
  std::ignore = handle_.release();
#elif BUILDFLAG(IS_APPLE)
  std::ignore = mach_send_.release();
  std::ignore = mach_receive_.release();
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::ignore = fd_.release();
#endif

#if BUILDFLAG(IS_ANDROID)
  std::ignore = binder_.release();
#endif
}

PlatformHandle PlatformHandle::Clone() const {
#if BUILDFLAG(IS_WIN)
  return PlatformHandle(CloneHandle(handle_));
#elif BUILDFLAG(IS_FUCHSIA)
  if (is_valid_handle())
    return PlatformHandle(CloneHandle(handle_));
  return PlatformHandle(CloneFD(fd_));
#elif BUILDFLAG(IS_APPLE)
  if (is_valid_mach_send())
    return PlatformHandle(CloneMachPort(mach_send_));
  CHECK(!is_valid_mach_receive()) << "Cannot clone Mach receive rights";
  return PlatformHandle(CloneFD(fd_));
#elif BUILDFLAG(IS_POSIX)
#if BUILDFLAG(IS_ANDROID)
  if (is_valid_binder()) {
    return PlatformHandle(binder_);
  }
#endif
  return PlatformHandle(CloneFD(fd_));
#endif
}

}  // namespace mojo
