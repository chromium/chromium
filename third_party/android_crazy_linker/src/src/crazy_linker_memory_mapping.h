// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_MEMORY_MAPPING_H
#define CRAZY_LINKER_MEMORY_MAPPING_H

#include <errno.h>
#include <sys/mman.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_error.h"

namespace crazy {

// Helper class for a scoped memory mapping. Usage is the following:
//
//  1) Call MemoryMapping::Allocate() to allocate a new memory mapping
//     and use the IsValid() method on the result to determine success or
//     failure.
//
//  2) Alternatively, use the constructor to take ownership of an existing
//     memory map range.
//
//  3) Destructor will always unmap the memory range, unless Release() was
//     called before.
//
class MemoryMapping {
 public:
  enum Protection {
    CAN_READ = PROT_READ,
    CAN_WRITE = PROT_WRITE,
    CAN_READ_WRITE = PROT_READ | PROT_WRITE
  };

  // Construct an empty instance.
  MemoryMapping() = default;

  // Construct an instance that takes ownership of an existing memory map.
  MemoryMapping(void* address, size_t size) : map_(address), size_(size) {}

  // Destructor will unmap the memory if it wasn't deallocated or released.
  ~MemoryMapping();

  // Disallow copies.
  MemoryMapping(const MemoryMapping&) = delete;
  MemoryMapping& operator=(const MemoryMapping&) = delete;

  // Allow move operations.
  MemoryMapping(MemoryMapping&& other) : map_(other.map_), size_(other.size_) {
    other.map_ = nullptr;
    other.size_ = 0;
  }
  MemoryMapping& operator=(MemoryMapping&& other);

  // Returns true iff the instance is valid, i.e. there is a mapped segment.
  bool IsValid() const { return map_ != nullptr; }

  void* address() const { return map_; }
  size_t size() const { return size_; }

  // Deallocate existing mapping, if any, now.
  void Deallocate();

  // Release the mapping from the instance. The caller is responsible for
  // calling ::munmap() on the previous value of address(), using the previous
  // value of size() as the second parameter.
  void Release() {
    map_ = nullptr;
    size_ = 0;
  }

  // Allocate a new mapping.
  // |address| is either NULL or a fixed memory address.
  // |size| is the page-aligned size, must be > 0.
  // |prot| are the desired protection bit flags.
  // |fd| is -1 (for anonymous mappings), or a valid file descriptor.
  // Return a new valid instance on success. On failure, return an invalid
  // instance and sets errno.
  static MemoryMapping Create(void* address,
                              size_t size,
                              Protection prot,
                              int fd);

  // Change the protection flags of the mapping.
  // On failure, return false and sets errno.
  bool SetProtection(Protection prot) {
    return (map_ && ::mprotect(map_, size_, static_cast<int>(prot)) == 0);
  }

 protected:
  void* map_ = nullptr;
  size_t size_ = 0;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_MEMORY_MAPPING_H
