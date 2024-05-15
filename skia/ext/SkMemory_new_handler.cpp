// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>

#include <algorithm>
#include <tuple>

#include "base/debug/alias.h"
#include "base/process/memory.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/private/base/SkMalloc.h"

#if BUILDFLAG(IS_WIN)
#include <malloc.h>
#include <windows.h>
#elif BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
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
    SkDEBUGFAIL("sk_out_of_memory");
    base::TerminateBecauseOutOfMemory(0);
    // Extra safety abort().
    abort();
}

void* sk_realloc_throw(void* addr, size_t size) {
    // This is the "normal" behavior of realloc(), but per man malloc(3), POSIX
    // compliance doesn't require it. Skia does though, starting with
    // https://skia-review.googlesource.com/c/skia/+/647456.
    if (size == 0) {
        sk_free(addr);
        return nullptr;
    }

    // TODO(crbug.com/340895215): there is no base::UncheckedRealloc, so we need
    // to rely on the built-in allocator. Mixing allocators also trips up UBSAN.
#if defined(UNDEFINED_SANITIZER)
    // It's slower to use alloc + free instead of realloc, but avoids mixing up
    // our allocators, which should placate UBSAN.
    size_t old_size = sk_malloc_size(addr, 0);
    void* result = sk_malloc_throw(size);
    sk_careful_memcpy(result, addr, std::min(size, old_size));
    sk_free(addr);
    return result;
#else
    return throw_on_failure(size, realloc(addr, size));
#endif
}

void sk_free(void* p) {
    if (p) {
#if BUILDFLAG(IS_IOS)
        free(p);
#else
        base::UncheckedFree(p);
#endif
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

static void* malloc_nothrow(size_t size, int debug_sentinel) {
  // TODO(b.kelemen): we should always use UncheckedMalloc but currently it
  // doesn't work as intended everywhere.
  void* result;
#if BUILDFLAG(IS_IOS)
  result = malloc(size);
#else
  // It's the responsibility of the caller to check the return value.
  std::ignore = base::UncheckedMalloc(size, &result);
#endif
  if (result) {
    prevent_overcommit(debug_sentinel, size, result);
  }
  return result;
}

static void* malloc_throw(size_t size, int debug_sentinel) {
  return throw_on_failure(size, malloc_nothrow(size, debug_sentinel));
}

static void* calloc_nothrow(size_t size) {
  // TODO(b.kelemen): we should always use UncheckedCalloc but currently it
  // doesn't work as intended everywhere.
  void* result;
#if BUILDFLAG(IS_IOS)
  result = calloc(1, size);
#else
  // It's the responsibility of the caller to check the return value.
  std::ignore = base::UncheckedCalloc(size, 1, &result);
#endif
  if (result) {
    prevent_overcommit(0, size, result);
  }
  return result;
}

static void* calloc_throw(size_t size) {
  return throw_on_failure(size, calloc_nothrow(size));
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
      return malloc_throw(size, /*debug_sentinel=*/0x42);
    } else {
      return malloc_nothrow(size, /*debug_sentinel=*/0x47);
    }
  }
}

size_t sk_malloc_size(void* addr, size_t size) {
  if (!addr) {
    return 0;
  }

  size_t completeSize = 0;

#if BUILDFLAG(IS_WIN)
  completeSize = _msize(addr);
#elif BUILDFLAG(IS_APPLE)
  completeSize = malloc_size(addr);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  completeSize = malloc_usable_size(addr);
#endif

  // Guarantee that we return at least `size`
  return std::max(completeSize, size);
}
