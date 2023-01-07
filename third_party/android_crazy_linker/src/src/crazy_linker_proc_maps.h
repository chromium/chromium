// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_PROC_MAPS_H
#define CRAZY_LINKER_PROC_MAPS_H

// Helper classes and functions to extract useful information out of
// /proc/self/maps.

#include "crazy_linker_util.h"

#include <stdint.h>
#include <sys/mman.h>  // for PROT_READ etc...

namespace crazy {

class ProcMapsInternal;

class ProcMaps {
 public:
  // Create instance. This opens and parses /proc/self/maps.
  // There is no error reporting. If the file can't be opened, then
  // entries() will be empty.
  ProcMaps();

  ~ProcMaps();

  // Small structure to model an entry.
  struct Entry {
    size_t vma_start;
    size_t vma_end;
    int prot_flags;
    size_t load_offset;
    const char* path;  // can be NULL, not always zero-terminated.
    size_t path_len;   // 0 if |path| is NULL.
  };

  const Vector<Entry>& entries() const { return entries_; }

  // Find entry containing a given |address|, or return nullptr.
  const Entry* FindEntryForAddress(void* address) const;

  // Find first entry matching a given |file_name|, or return nullptr.
  // If |file_name| contains a slash, this will try to perform an
  // exact match with the content of /proc/self/maps. Otherwise,
  // it will be taken as a base name, and the load address of the first
  // matching entry will be returned.
  const Entry* FindEntryForFile(const char* file_name) const;

 private:
  Vector<Entry> entries_;
};

// Find which loaded ELF binary contains |address|.
// On success, returns true and sets |*load_address| to its load address,
// and fills |path_buffer| with the path to the corresponding file.
bool FindElfBinaryForAddress(void* address,
                             uintptr_t* load_address,
                             char* path_buffer,
                             size_t path_buffer_len);

// Return the load address of a given ELF binary.
// If |file_name| contains a slash, this will try to perform an
// exact match with the content of /proc/self/maps. Otherwise,
// it will be taken as a base name, and the load address of the first
// matching entry will be returned.
// On success, returns true and sets |*load_address| to the load address,
// and |*load_offset| to the load offset.
bool FindLoadAddressForFile(const char* file_name,
                            uintptr_t* load_address,
                            uintptr_t* load_offset);

}  // namespace crazy

#endif  // CRAZY_LINKER_PROC_MAPS_H
