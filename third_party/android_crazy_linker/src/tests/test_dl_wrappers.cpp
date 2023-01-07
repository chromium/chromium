// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libzoo.so) with the linker.
// - Find the address of the "Zoo" function in libzoo.so.
// - Call the Zoo() function, which will use dlopen() / dlsym() to
//   find libbar.so (which depends on libfoo.so).
// - Close the library.

// This tests the dlopen/dlsym/dlclose wrappers provided by the crazy
// linker to loaded libraries.

#include <stdio.h>
#include <crazy_linker.h>

#include "test_util.h"

typedef bool (*FunctionPtr)();

#define LIB_NAME "libcrazy_linker_tests_libzoo.so"

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Load libzoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Zoo" symbol.
  FunctionPtr zoo_func;
  if (!crazy_library_find_symbol(
           library, "Zoo", reinterpret_cast<void**>(&zoo_func))) {
    Panic("Could not find 'Zoo' in " LIB_NAME "\n");
  }

  // Call it.
  bool ret = (*zoo_func)();
  if (!ret)
    Panic("'Zoo' function failed!");

  // Close the library.
  printf("Closing " LIB_NAME "\n");
  crazy_library_close(library);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
