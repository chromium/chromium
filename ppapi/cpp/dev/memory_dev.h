// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_MEMORY_DEV_H_
#define PPAPI_CPP_DEV_MEMORY_DEV_H_

#include "ppapi/c/pp_stdint.h"

/// @file
/// This file defines APIs related to memory management.

namespace pp {

/// APIs related to memory management, time, and threads.
class Memory_Dev {
 public:
  Memory_Dev() {}

  /// A function that allocates memory.
  ///
  /// @param[in] num_bytes A number of bytes to allocate.
  /// @return A pointer to the memory if successful, NULL If the
  /// allocation fails.
  void* MemAlloc(uint32_t num_bytes);

  /// A function that deallocates memory.
  ///
  /// @param[in] ptr A pointer to the memory to deallocate. It is safe to
  /// pass NULL to this function.
  void MemFree(void* ptr);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_MEMORY_DEV_H_
