// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_SCOPED_IPCZ_HANDLE_H_
#define MOJO_CORE_SCOPED_IPCZ_HANDLE_H_

#include <utility>

#include "mojo/core/system_impl_export.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

// ScopedIpczHandle implements unique ownership of an IpczHandle. This object
// closes the underlying handle value on destruction if it hasn't been closed
// yet.
class MOJO_SYSTEM_IMPL_EXPORT ScopedIpczHandle {
 public:
  ScopedIpczHandle();
  explicit ScopedIpczHandle(IpczHandle handle);
  ScopedIpczHandle(ScopedIpczHandle&&);
  ScopedIpczHandle(const ScopedIpczHandle&) = delete;
  ScopedIpczHandle& operator=(ScopedIpczHandle&&);
  ScopedIpczHandle& operator=(const ScopedIpczHandle&) = delete;
  ~ScopedIpczHandle();

  bool is_valid() const { return handle_ != IPCZ_INVALID_HANDLE; }
  IpczHandle get() const { return handle_; }

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
