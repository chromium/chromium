// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_memory_mapping.h"

namespace crazy {

MemoryMapping::~MemoryMapping() {
  Deallocate();
}

MemoryMapping& MemoryMapping::operator=(MemoryMapping&& other) {
  if (this != &other) {
    Deallocate();
    map_ = other.map_;
    size_ = other.size_;
    other.map_ = nullptr;
    other.size_ = 0;
  }
  return *this;
}

void MemoryMapping::Deallocate() {
  if (map_) {
    ::munmap(map_, size_);
    map_ = nullptr;
    size_ = 0;
  }
}

// static
MemoryMapping MemoryMapping::Create(void* address,
                                    size_t size,
                                    Protection prot,
                                    int fd) {
  int flags = (fd >= 0) ? MAP_SHARED : MAP_ANONYMOUS;
  if (address)
    flags |= MAP_FIXED;

  void* map = ::mmap(address, size, static_cast<int>(prot), flags, fd, 0);
  if (map == MAP_FAILED) {
    map = nullptr;
    size = 0;
  }
  return {map, size};
}

}  // namespace crazy
