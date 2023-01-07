// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libbar.so) with the linker, which depends on
//   another library (libfoo.so)
// - Find the address of the "Bar" function in libbar.so.
// - Call the Bar() function, which ends up calling Foo() in libfoo.so
// - Close the library.

#include <stdio.h>
#include <crazy_linker.h>

#include "test_util.h"

#define LIB_NAME "libcrazy_linker_tests_libbar.so"

typedef void (*FunctionPtr)();

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // DEBUG
  crazy_context_set_load_address(context, 0x20000000);

  // Load libbar.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Bar" symbol.
  FunctionPtr bar_func;
  if (!crazy_library_find_symbol(
           library, "Bar", reinterpret_cast<void**>(&bar_func))) {
    Panic("Could not find 'Bar' in %s\n", LIB_NAME);
  }

  // Call it.
  (*bar_func)();

  // Find the "Foo" symbol from libbar.so
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(
           library, "Foo", reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' from %s\n", LIB_NAME);
  }

  // Close the library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
