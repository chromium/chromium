// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_RELRO_H
#define CRAZY_LINKER_ELF_RELRO_H

#include "crazy_linker_ashmem.h"
#include "crazy_linker_memory_mapping.h"
#include "crazy_linker_system.h"
#include "elf_traits.h"

namespace crazy {

class ElfView;

// A class used to model a shared RELRO section backed by an Ashmem region.
// The region is always owned by the SharedRelro, unless DetachFd() is called.
// The SharedRelro may or may not be mapped into the process.
class SharedRelro {
 public:
  // Create a new SharedRelro. Note that the object becomes the owner of
  // |ashmem_fd|, unless DetachFd() is called after this.
  SharedRelro() : start_(0), size_(0), ashmem_() {}

  ~SharedRelro() {}

  size_t start() const { return start_; }
  size_t end() const { return start_ + size_; }
  size_t size() const { return size_; }
  int fd() const { return ashmem_.fd(); }

  // Return the ashmem region's file descriptor, and detach it from the object.
  // After this call, fd() will always return -1.
  int DetachFd() { return ashmem_.Release(); }

  // Allocate a new ashmem region of |relro_size| bytes for |library_name|.
  // This operation doesn't change the process' mappings. On error, return
  // false and set |error| message.
  bool Allocate(size_t relro_size, const char* library_name, Error* error);

  // Copy the content of the current process' RELRO into the ashmem region.
  // |relro_start| is the RELRO address (page-aligned).
  // |relro_size| is the RELRO size in bytes (page-aligned), and must match
  // the allocation size passed to Allocate().
  // On failure, return false and set |error| message.
  bool CopyFrom(size_t relro_start, size_t relro_size, Error* error);

  // Copy the contents of the current process' RELRO into the ashmem region
  // but adjust any relocation targets within it to correspond to a new
  // |load_address|. |view| must point to a mapped ELF binary for the current
  // library. |relro_start| corresponds to the address of the current
  // process' RELRO, i.e. is not relocated.
  bool CopyFromRelocated(const ElfView* view,
                         size_t load_address,
                         size_t relro_start,
                         size_t relro_size,
                         Error* error);

  // Force the section to be read-only.
  bool ForceReadOnly(Error* error);

  // Map the ashmem region's pages into the current process, doing a comparison
  // to avoid corrupting parts of the RELRO section that are different in this
  // one (e.g. due to symbolic relocations to randomized system libraries).
  // This operation is _not_ atomic, i.e. no other thread should try to execute
  // code that reads from the RELRO region during this call.
  // On failure, return false and set |error| message.
  // This operation does not transfer ownership of |ashmem_fd| to the object.
  bool InitFrom(size_t relro_start,
                size_t relro_size,
                int ashmem_fd,
                Error* error);

 private:
  size_t start_;
  size_t size_;
  AshmemRegion ashmem_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_RELRO_H
