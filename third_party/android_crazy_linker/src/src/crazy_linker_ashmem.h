// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ASHMEM_H
#define CRAZY_LINKER_ASHMEM_H

#include <unistd.h>

namespace crazy {

// Helper class to hold a scoped ashmem region file descriptor.
class AshmemRegion {
 public:
  AshmemRegion() : fd_(-1) {}

  ~AshmemRegion() { Reset(-1); }

  AshmemRegion(const AshmemRegion& other) = delete;
  AshmemRegion& operator=(const AshmemRegion& other) = delete;

  AshmemRegion(AshmemRegion&& other) : fd_(other.fd_) { other.fd_ = -1; }

  AshmemRegion& operator=(AshmemRegion&& other) {
    if (this != &other) {
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  int fd() const { return fd_; }

  int Release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

  void Reset(int fd) {
    if (fd_ != -1) {
      ::close(fd_);
    }
    fd_ = fd;
  }

  // Try to allocate a new ashmem region of |region_size|
  // (page-aligned) bytes. |region_name| is optional, if not NULL
  // it will be the name of the region (only used for debugging).
  // Returns true on success, false otherwise.
  bool Allocate(size_t region_size, const char* region_name);

  // Change the protection flags of the region. Returns true on success.
  // On failure, check errno for an error code.
  bool SetProtectionFlags(int prot_flags);

  // Check that the region tied to file descriptor |fd| is properly read-only:
  // I.e. that it cannot be mapped writable, or that a read-only mapping cannot
  // be mprotect()-ed into MAP_WRITE. On failure, return false and sets errno.
  //
  // See:
  //   http://www.cvedetails.com/cve/CVE-2011-1149/
  // And kernel patch at:
  //   https://android.googlesource.com/kernel/common.git/+/
  //     56f76fc68492af718fff88927bc296635d634b78%5E%21/
  static bool CheckFileDescriptorIsReadOnly(int fd);

 private:
  int fd_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ASHMEM_H
