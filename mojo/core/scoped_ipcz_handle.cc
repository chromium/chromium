// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/scoped_ipcz_handle.h"

#include <utility>

#include "base/check_op.h"
#include "mojo/core/ipcz_api.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

ScopedIpczHandle::ScopedIpczHandle() = default;

ScopedIpczHandle::ScopedIpczHandle(IpczHandle handle) : handle_(handle) {}

ScopedIpczHandle::ScopedIpczHandle(ScopedIpczHandle&& other)
    : handle_(std::exchange(other.handle_, IPCZ_INVALID_HANDLE)) {}

ScopedIpczHandle& ScopedIpczHandle::operator=(ScopedIpczHandle&& other) {
  reset();
  handle_ = std::exchange(other.handle_, IPCZ_INVALID_HANDLE);
  return *this;
}

ScopedIpczHandle::~ScopedIpczHandle() {
  reset();
}

void ScopedIpczHandle::reset() {
  if (is_valid()) {
    const IpczResult result = GetIpczAPI().Close(
        std::exchange(handle_, IPCZ_INVALID_HANDLE), IPCZ_NO_FLAGS, nullptr);
    DCHECK_EQ(result, IPCZ_RESULT_OK);
  }
}

}  // namespace mojo::core
