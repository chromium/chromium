// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_relro.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "crazy_linker_elf_relocations.h"
#include "crazy_linker_elf_view.h"
#include "crazy_linker_memory_mapping.h"
#include "crazy_linker_util.h"

namespace crazy {

namespace {

inline bool PageEquals(const char* p1, const char* p2) {
  return ::memcmp(p1, p2, PAGE_SIZE) == 0;
}

// Swap pages between |addr| and |addr + size| with the bytes
// from the ashmem region identified by |fd|, starting from
// a given |offset|. On failure return false and set |error| message.
bool SwapPagesFromFd(void* addr,
                     size_t size,
                     int fd,
                     size_t offset,
                     Error* error) {
  // Unmap current pages.
  if (::munmap(addr, size) < 0) {
    error->Format("%s: Could not unmap %p-%p: %s",
                  __FUNCTION__,
                  addr,
                  (char*)addr + size,
                  strerror(errno));
    return false;
  }

  // Remap the fd pages at the same location now.
  void* new_map = ::mmap(addr,
                         size,
                         PROT_READ,
                         MAP_FIXED | MAP_SHARED,
                         fd,
                         static_cast<off_t>(offset));
  if (new_map == MAP_FAILED) {
    char* p = reinterpret_cast<char*>(addr);
    error->Format("%s: Could not map %p-%p: %s",
                  __FUNCTION__,
                  p,
                  p + size,
                  strerror(errno));
    return false;
  }

// TODO(digit): Is this necessary?
#ifdef __arm__
  __clear_cache(addr, (char*)addr + size);
#endif

  // Done.
  return true;
}

}  // namespace

bool SharedRelro::Allocate(size_t relro_size,
                           const char* library_name,
                           Error* error) {
  // Allocate a new ashmem region.
  String name("RELRO:");
  name += library_name;
  if (!ashmem_.Allocate(relro_size, name.c_str())) {
    error->Format("Could not allocate RELRO ashmem region for %s: %s",
                  library_name,
                  strerror(errno));
    return false;
  }

  start_ = 0;
  size_ = relro_size;
  return true;
}

bool SharedRelro::CopyFrom(size_t relro_start,
                           size_t relro_size,
                           Error* error) {
  // Map it in the process.
  MemoryMapping map = MemoryMapping::Create(
      nullptr, relro_size, MemoryMapping::CAN_WRITE, ashmem_.fd());
  if (!map.IsValid()) {
    error->Format("Could not allocate RELRO mapping: %s", strerror(errno));
    return false;
  }

  // Copy process' RELRO into it.
  ::memcpy(map.address(), reinterpret_cast<void*>(relro_start), relro_size);

  // Unmap it.
  map.Deallocate();

  // Everything's good.
  start_ = relro_start;
  size_ = relro_size;
  return true;
}

bool SharedRelro::CopyFromRelocated(const ElfView* view,
                                    size_t load_address,
                                    size_t relro_start,
                                    size_t relro_size,
                                    Error* error) {
  // Offset of RELRO section in current library.
  size_t relro_offset = relro_start - view->load_address();

  ElfRelocations relocations;
  if (!relocations.Init(view, error))
    return false;

  // Map the region in memory (any address).
  MemoryMapping map = MemoryMapping::Create(
      nullptr, relro_size, MemoryMapping::CAN_READ_WRITE, ashmem_.fd());
  if (!map.IsValid()) {
    error->Format("Could not allocate RELRO mapping for: %s", strerror(errno));
    return false;
  }

  // Copy and relocate.
  relocations.CopyAndRelocate(relro_start,
                              reinterpret_cast<size_t>(map.address()),
                              load_address + relro_offset, relro_size);
  // Unmap it.
  map.Deallocate();
  start_ = load_address + relro_offset;
  size_ = relro_size;
  return true;
}

bool SharedRelro::ForceReadOnly(Error* error) {
  // Ensure the ashmem region content isn't writable anymore.
  if (!ashmem_.SetProtectionFlags(PROT_READ)) {
    error->Format("Could not make RELRO ashmem region read-only: %s",
                  strerror(errno));
    return false;
  }
  return true;
}

bool SharedRelro::InitFrom(size_t relro_start,
                           size_t relro_size,
                           int ashmem_fd,
                           Error* error) {
  // Create temporary mapping of the ashmem region.
  LOG("Entering addr=%p size=%p fd=%d", (void*)relro_start, (void*)relro_size,
      ashmem_fd);

  // Sanity check: Ashmem file descriptor must be read-only.
  if (!AshmemRegion::CheckFileDescriptorIsReadOnly(ashmem_fd)) {
    error->Format("Ashmem file descriptor is not read-only: %s",
                  strerror(errno));
    return false;
  }

  MemoryMapping fd_map = MemoryMapping::Create(
      nullptr, relro_size, MemoryMapping::CAN_READ, ashmem_fd);
  if (!fd_map.IsValid()) {
    error->Format("Cannot map RELRO ashmem region as read-only: %s",
                  strerror(errno));
    return false;
  }

  LOG("mapping allocated at %p", fd_map.address());

  char* cur_page = reinterpret_cast<char*>(relro_start);
  char* fd_page = static_cast<char*>(fd_map.address());
  size_t p = 0;
  size_t size = relro_size;
  size_t similar_size = 0;

  do {
    // Skip over dissimilar pages.
    while (p < size && !PageEquals(cur_page + p, fd_page + p)) {
      p += PAGE_SIZE;
    }

    // Count similar pages.
    size_t p2 = p;
    while (p2 < size && PageEquals(cur_page + p2, fd_page + p2)) {
      p2 += PAGE_SIZE;
    }

    if (p2 > p) {
      // Swap pages between |pos| and |pos2|.
      LOG("Swap pages at %p-%p", cur_page + p, cur_page + p2);
      if (!SwapPagesFromFd(cur_page + p, p2 - p, ashmem_fd, p, error))
        return false;

      similar_size += (p2 - p);
    }

    p = p2;
  } while (p < size);

  LOG("Swapped %d pages over %d (%d %%, %d KB not shared)",
      similar_size / PAGE_SIZE, size / PAGE_SIZE, similar_size * 100 / size,
      (size - similar_size) / 4096);

  if (similar_size == 0) {
    error->Format("No pages were swapped into RELRO ashmem");
    return false;
  }

  start_ = relro_start;
  size_ = relro_size;
  return true;
}

}  // namespace crazy
