// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SANITIZERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SANITIZERS_H_

// TODO(sof): Add SyZyASan support?
#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#define ASAN_REGION_IS_POISONED(addr, size) \
  __asan_region_is_poisoned(addr, size)
#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_REGION_IS_POISONED(addr, size) \
  ((void)(addr), (void)(size), (void*)nullptr)
#define NO_SANITIZE_ADDRESS
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#else
#define __lsan_register_root_region(addr, size) ((void)(addr), (void)(size))
#define __lsan_unregister_root_region(addr, size) ((void)(addr), (void)(size))
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#define NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
#else
#define NO_SANITIZE_MEMORY
#endif

#if defined(THREAD_SANITIZER)
#define NO_SANITIZE_THREAD __attribute__((no_sanitize_thread))
#else
#define NO_SANITIZE_THREAD
#endif

#if defined(__clang__)
#define NO_SANITIZE_HWADDRESS __attribute__((no_sanitize("hwaddress")))
#else
#define NO_SANITIZE_HWADDRESS
#endif

// NO_SANITIZE_UNRELATED_CAST - Disable runtime checks related to casts between
// unrelated objects (-fsanitize=cfi-unrelated-cast or -fsanitize=vptr).
#if defined(__clang__)
#define NO_SANITIZE_UNRELATED_CAST \
  __attribute__((no_sanitize("cfi-unrelated-cast", "vptr")))
#define NO_SANITIZE_CFI_ICALL __attribute__((no_sanitize("cfi-icall")))
#else
#define NO_SANITIZE_UNRELATED_CAST
#define NO_SANITIZE_CFI_ICALL
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SANITIZERS_H_
