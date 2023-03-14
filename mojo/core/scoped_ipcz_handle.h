// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_SCOPED_IPCZ_HANDLE_H_
#define MOJO_CORE_SCOPED_IPCZ_HANDLE_H_

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "mojo/core/system_impl_export.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

// ScopedIpczHandle implements unique ownership of an IpczHandle. This object
// closes the underlying handle value on destruction if it hasn't been closed
// yet.
class MOJO_SYSTEM_IMPL_EXPORT ScopedIpczHandle {
 public:
  // Receiver is an adapter which can be used to pass a ScopedIpczHandle as an
  // IpczHandle*, for interfacing with C APIs that return handles through
  // output parameters. For some function like:
  //
  //     Foo(IpczHandle* out_handle)
  //
  // We can receive Foo's output into a ScopedIpczHandles with something like:
  //
  //     ScopedIpczHandle foo;
  //     Foo(ScopedIpczHandle::Receiver(foo))
  //
  class Receiver {
   public:
    explicit Receiver(ScopedIpczHandle& target) : target_(target) {}

    ~Receiver() {
      if (received_handle_ != IPCZ_INVALID_HANDLE) {
        (*target_) = ScopedIpczHandle(received_handle_);
      }
    }

    operator IpczHandle*() { return &received_handle_; }

   private:
    const raw_ref<ScopedIpczHandle> target_;
    IpczHandle received_handle_ = IPCZ_INVALID_HANDLE;
  };

  ScopedIpczHandle();
  explicit ScopedIpczHandle(IpczHandle handle);
  ScopedIpczHandle(ScopedIpczHandle&&);
  ScopedIpczHandle(const ScopedIpczHandle&) = delete;
  ScopedIpczHandle& operator=(ScopedIpczHandle&&);
  ScopedIpczHandle& operator=(const ScopedIpczHandle&) = delete;
  ~ScopedIpczHandle();

  bool is_valid() const { return handle_ != IPCZ_INVALID_HANDLE; }
  const IpczHandle& get() const { return handle_; }

  // Resets this object to an invalid handle, closing the previously held handle
  // if it was valid.
  void reset();

  // Releases ownership of the underlying IpczHandle and returns its value.
  [[nodiscard]] IpczHandle release() {
    return std::exchange(handle_, IPCZ_INVALID_HANDLE);
  }

 private:
  IpczHandle handle_ = IPCZ_INVALID_HANDLE;
};

static_assert(sizeof(IpczHandle) == sizeof(ScopedIpczHandle),
              "ScopedIpczHandle must be the same size as IpczHandle.");

}  // namespace mojo::core

#endif  // MOJO_CORE_SCOPED_IPCZ_HANDLE_H_
