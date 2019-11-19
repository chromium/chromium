// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libfoo.so) with the linker.
// - Find the address of the "Foo" function in it.
// - Call the function.
// - Close the library.
#include <crazy_linker.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "test_util.h"

typedef void (*FunctionPtr)();

#ifndef LIB_NAME
#define LIB_NAME "libcrazy_linker_tests_libfoo.so"
#endif

std::string GetProgramDirectory(const char* argv0) {
  const char* sep = strrchr(argv0, '/');
  if (!sep) {
    return ".";
  }
  return std::string(argv0, sep - argv0);
}

int main(int argc, const char** argv) {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Allocate a memory map range large enough for our library.
  const size_t kMapSize = 1024 * 1024;  // 1 MiB should be enough.
  void* reserved_map =
      mmap(nullptr, kMapSize, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (reserved_map == MAP_FAILED)
    Panic("mmap() failed: %s", strerror(errno));

  crazy_context_set_reserved_map(
      context, reinterpret_cast<uintptr_t>(reserved_map), kMapSize, false);

  // Load libfoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Foo" symbol.
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(library, "Foo",
                                 reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' in %s\n", LIB_NAME);
  }

  // Call it.
  (*foo_func)();

  // Close the library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  // Check that the memory map is still there by writing to it.
  printf("Erasing memory map\n");
  if (mprotect(reserved_map, kMapSize, PROT_WRITE) < 0)
    Panic("Could not mprotect() range: %s", strerror(errno));
  memset(reserved_map, 1, kMapSize);

  printf("Trying to reload inside smaller reserved map (no fallback).\n");
  // Try to load the library again at the same address, without a reserved
  // size. This should fail.
  crazy_context_set_reserved_map(
      context, reinterpret_cast<uintptr_t>(reserved_map), 4096, false);

  if (crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could unexpectedly load library in small mapping!");
  }

  printf("Trying to reload inside smaller reserved map (with fallback)\n.");
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not reload library with fallback in small mapping!");
  }
  crazy_library_close(library);

  printf("Unmapping reserved mapping.\n");
  if (munmap(reserved_map, kMapSize) < 0)
    Panic("Could not unmap reserved mapping: %s", strerror(errno));

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
