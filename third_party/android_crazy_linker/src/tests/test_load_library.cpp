// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libfoo.so) with the linker.
// - Find the address of the "Foo" function in it.
// - Call the function.
// - Close the library.

#include <stdio.h>
#include <crazy_linker.h>

#include "test_util.h"

typedef void (*FunctionPtr)();

#ifndef LIB_NAME
#define LIB_NAME "libcrazy_linker_tests_libfoo.so"
#endif

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // DEBUG
  crazy_context_set_load_address(context, 0x20000000);

  // Load libfoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Foo" symbol.
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(
           library, "Foo", reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' in %s\n", LIB_NAME);
  }

  // Call it.
  (*foo_func)();

  // Close the library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
