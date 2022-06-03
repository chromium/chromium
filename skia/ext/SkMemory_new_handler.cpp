// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>

#include "base/debug/alias.h"
#include "base/macros.h"
#include "base/process/memory.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/private/SkMalloc.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

// This implementation of sk_malloc_flags() and friends is similar to
// SkMemory_malloc.cpp, except it uses base::UncheckedMalloc and friends
// for non-SK_MALLOC_THROW calls.
//
// The name of this file is historic: a previous implementation tried to
// use std::set_new_handler() for the same effect, but it didn't actually work.

static inline void* throw_on_failure(size_t size, void* p) {
    if (size > 0 && p == NULL) {
        // If we've got a NULL here, the only reason we should have failed is running out of RAM.
        sk_out_of_memory();
    }
    return p;
}

void sk_abort_no_print() {
    // Linker's ICF feature may merge this function with other functions with
    // the same definition (e.g. any function whose sole job is to call abort())
    // and it may confuse the crash report processing system.
    // http://crbug.com/860850
    static int static_variable_to_make_this_function_unique = 0x736b;  // "sk"
    base::debug::Alias(&static_variable_to_make_this_function_unique);

    abort();
}

void sk_out_of_memory(void) {
    SkASSERT(!"sk_out_of_memory");
    base::TerminateBecauseOutOfMemory(0);
    // Extra safety abort().
    abort();
}

void* sk_realloc_throw(void* addr, size_t size) {
    return throw_on_failure(size, realloc(addr, size));
}

void sk_free(void* p) {
    if (p) {
        free(p);
    }
}

// We get lots of bugs filed on us that amount to overcommiting bitmap memory,
// then some time later failing to back that VM with physical memory.
// They're hard to track down, so in Debug mode we touch all memory right up front.
//
// For malloc, fill is an arbitrary byte and ideally not 0.  For calloc, it's got to be 0.
static void* prevent_overcommit(int fill, size_t size, void* p) {
    // We probably only need to touch one byte per page, but memset makes things easy.
    SkDEBUGCODE(memset(p, fill, size));
    return p;
}

static void* malloc_throw(size_t size) {
  return prevent_overcommit(0x42, size, throw_on_failure(size, malloc(size)));
}

static void* malloc_nothrow(size_t size) {
  // TODO(b.kelemen): we should always use UncheckedMalloc but currently it
  // doesn't work as intended everywhere.
  void* result;
#if  defined(OS_IOS)
    result = malloc(size);
#else
    // It's the responsibility of the caller to check the return value.
    ignore_result(base::UncheckedMalloc(size, &result));
#endif
    if (result) {
        prevent_overcommit(0x47, size, result);
    }
    return result;
}

static void* calloc_throw(size_t size) {
  return prevent_overcommit(0, size, throw_on_failure(size, calloc(size, 1)));
}

static void* calloc_nothrow(size_t size) {
  // TODO(b.kelemen): we should always use UncheckedCalloc but currently it
  // doesn't work as intended everywhere.
  void* result;
#if  defined(OS_IOS)
    result = calloc(1, size);
#else
    // It's the responsibility of the caller to check the return value.
    ignore_result(base::UncheckedCalloc(size, 1, &result));
#endif
    if (result) {
        prevent_overcommit(0, size, result);
    }
    return result;
}

void* sk_malloc_flags(size_t size, unsigned flags) {
  if (flags & SK_MALLOC_ZERO_INITIALIZE) {
    if (flags & SK_MALLOC_THROW) {
      return calloc_throw(size);
    } else {
      return calloc_nothrow(size);
    }
  } else {
    if (flags & SK_MALLOC_THROW) {
      return malloc_throw(size);
    } else {
      return malloc_nothrow(size);
    }
  }
}
