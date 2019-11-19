// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_H_

#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/public/c/system/platform_handle.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#elif defined(OS_FUCHSIA)
#include <lib/zx/handle.h>
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/scoped_mach_port.h"
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/files/scoped_file.h"
#endif

namespace mojo {

// A PlatformHandle is a generic wrapper around a platform-specific system
// handle type, e.g. a POSIX file descriptor, Windows HANDLE, or macOS Mach
// port. This can wrap any of various such types depending on the host platform
// for which it's compiled.
//
// This is useful primarily for two reasons:
//
// - Interacting with the Mojo invitation API, which use OS primitives to
//   bootstrap Mojo IPC connections.
// - Interacting with Mojo platform handle wrapping and unwrapping API, which
//   allows handles to OS primitives to be transmitted over Mojo IPC with a
//   stable wire representation via Mojo handles.
//
// NOTE: This assumes ownership if the handle it represents.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) PlatformHandle {
 public:
  enum class Type {
    kNone,
#if defined(OS_WIN) || defined(OS_FUCHSIA)
    kHandle,
#elif defined(OS_MACOSX) && !defined(OS_IOS)
    kMachSend,
    kMachReceive,
#endif
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    kFd,
#endif
  };

  PlatformHandle();
  PlatformHandle(PlatformHandle&& other);

#if defined(OS_WIN)
  explicit PlatformHandle(base::win::ScopedHandle handle);
#elif defined(OS_FUCHSIA)
  explicit PlatformHandle(zx::handle handle);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  explicit PlatformHandle(base::mac::ScopedMachSendRight mach_port);
  explicit PlatformHandle(base::mac::ScopedMachReceiveRight mach_port);
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  explicit PlatformHandle(base::ScopedFD fd);
#endif

  ~PlatformHandle();

  PlatformHandle& operator=(PlatformHandle&& other);

  // Takes ownership of |handle|'s underlying platform handle and fills in
  // |mojo_handle| with a representation of it. The caller assumes ownership of
  // the platform handle.
  static void ToMojoPlatformHandle(PlatformHandle handle,
                                   MojoPlatformHandle* mojo_handle);

  // Closes the underlying platform handle.
  // Assumes ownership of the platform handle described by |handle|, and returns
  // it as a new PlatformHandle.
  static PlatformHandle FromMojoPlatformHandle(
      const MojoPlatformHandle* handle);

  Type type() const { return type_; }

  void reset();

  // Relinquishes ownership of the underlying handle, regardless of type, and
  // discards its value. To release and obtain the underlying handle value, use
  // one of the specific |Release*()| methods below.
  void release();

  // Duplicates the underlying platform handle, returning a new PlatformHandle
  // which owns it.
  PlatformHandle Clone() const;

#if defined(OS_WIN)
  bool is_valid() const { return is_valid_handle(); }
  bool is_valid_handle() const { return handle_.IsValid(); }
  bool is_handle() const { return type_ == Type::kHandle; }
  const base::win::ScopedHandle& GetHandle() const { return handle_; }
  base::win::ScopedHandle TakeHandle() {
    DCHECK_EQ(type_, Type::kHandle);
    type_ = Type::kNone;
    return std::move(handle_);
  }
  HANDLE ReleaseHandle() WARN_UNUSED_RESULT {
    DCHECK_EQ(type_, Type::kHandle);
    type_ = Type::kNone;
    return handle_.Take();
  }
#elif defined(OS_FUCHSIA)
  bool is_valid() const { return is_valid_fd() || is_valid_handle(); }
  bool is_valid_handle() const { return handle_.is_valid(); }
  bool is_handle() const { return type_ == Type::kHandle; }
  const zx::handle& GetHandle() const { return handle_; }
  zx::handle TakeHandle() {
    if (type_ == Type::kHandle)
      type_ = Type::kNone;
    return std::move(handle_);
  }
  zx_handle_t ReleaseHandle() WARN_UNUSED_RESULT {
    if (type_ == Type::kHandle)
      type_ = Type::kNone;
    return handle_.release();
  }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  bool is_valid() const { return is_valid_fd() || is_valid_mach_port(); }
  bool is_valid_mach_port() const {
    return is_valid_mach_send() || is_valid_mach_receive();
  }

  bool is_valid_mach_send() const { return mach_send_.is_valid(); }
  bool is_mach_send() const { return type_ == Type::kMachSend; }
  const base::mac::ScopedMachSendRight& GetMachSendRight() const {
    return mach_send_;
  }
  base::mac::ScopedMachSendRight TakeMachSendRight() {
    if (type_ == Type::kMachSend)
      type_ = Type::kNone;
    return std::move(mach_send_);
  }
  mach_port_t ReleaseMachSendRight() WARN_UNUSED_RESULT {
    return TakeMachSendRight().release();
  }

  bool is_valid_mach_receive() const { return mach_receive_.is_valid(); }
  bool is_mach_receive() const { return type_ == Type::kMachReceive; }
  const base::mac::ScopedMachReceiveRight& GetMachReceiveRight() const {
    return mach_receive_;
  }
  base::mac::ScopedMachReceiveRight TakeMachReceiveRight() {
    if (type_ == Type::kMachReceive)
      type_ = Type::kNone;
    return std::move(mach_receive_);
  }
  mach_port_t ReleaseMachReceiveRight() WARN_UNUSED_RESULT {
    return TakeMachReceiveRight().release();
  }
#elif defined(OS_POSIX)
  bool is_valid() const { return is_valid_fd(); }
#else
#error "Unsupported platform."
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  bool is_valid_fd() const { return fd_.is_valid(); }
  bool is_fd() const { return type_ == Type::kFd; }
  const base::ScopedFD& GetFD() const { return fd_; }
  base::ScopedFD TakeFD() {
    if (type_ == Type::kFd)
      type_ = Type::kNone;
    return std::move(fd_);
  }
  int ReleaseFD() WARN_UNUSED_RESULT {
    if (type_ == Type::kFd)
      type_ = Type::kNone;
    return fd_.release();
  }
#endif

 private:
  Type type_ = Type::kNone;

#if defined(OS_WIN)
  base::win::ScopedHandle handle_;
#elif defined(OS_FUCHSIA)
  zx::handle handle_;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::ScopedMachSendRight mach_send_;
  base::mac::ScopedMachReceiveRight mach_receive_;
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  base::ScopedFD fd_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PlatformHandle);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_H_
