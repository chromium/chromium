// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LOAD_PARAMS_H
#define CRAZY_LINKER_LOAD_PARAMS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "crazy_linker_util.h"

namespace crazy {

// A structure used to hold parameters related to loading an ELF library
// into the current process' address space.
//
// |library_path| is either the full library path.
// |library_fd| is a file descriptor. If >= 0, the |library_path| is ignored.
// |library_offset| is the page-aligned offset where the library starts in
//   its input file (typically > 0 when reading from Android APKs).
// |wanted_address| is either 0, or the address where the library should
//   be loaded.
// |reserved_size| is either 0, or a page-aligned size in bytes corresponding
//   to a reserved memory area where to load the library, starting from
//   |wanted_address|.
// |reserved_load_fallback| is ignored if |reserved_size| is 0. Otherwise, a
//   value of true means that if the load fails at the reserved address range,
//   the linker will try again at a different address.
struct LoadParams {
  String library_path;
  int library_fd = -1;
  off_t library_offset = 0;
  uintptr_t wanted_address = 0;
  uintptr_t reserved_size = 0;
  bool reserved_load_fallback = false;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LOAD_PARAMS_H
