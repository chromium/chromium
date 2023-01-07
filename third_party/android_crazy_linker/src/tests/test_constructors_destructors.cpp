// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libcrazy_linker_tests_libfoo_with_static_constructor.so)
//   with the linker.\
//
// - This shall execute a static constructor that will change the value
//   of the TEST_VAR environment variable to "LOADED'.
//
// - Close the library, this shall execute a static destructor that will
//   change the value of TEST_VAR to "UNLOADED'.

#include <stdio.h>
#include <stdlib.h>

#include <crazy_linker.h>

#include "test_util.h"

#define LIB_NAME "libcrazy_linker_tests_libfoo_with_static_constructor.so"

typedef void (*FunctionPtr)();

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  setenv("TEST_VAR", "INIT", 1);

  // DEBUG
  crazy_context_set_load_address(context, 0x20000000);

  // Load libfoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  const char* env = getenv("TEST_VAR");
  if (!env || strcmp(env, "LOADED"))
    Panic("Constructors not run when loading " LIB_NAME "\n");

  printf("Constructors called when loading the library\n");

  // Find the "Foo" symbol.
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(
           library, "Foo", reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' in " LIB_NAME "\n");
  }

  // Call it.
  (*foo_func)();

  // Close the library.
  crazy_library_close(library);

  env = getenv("TEST_VAR");
  if (!env || strcmp(env, "UNLOADED"))
    Panic("Destructors not run when unloading " LIB_NAME "\n");

  printf("Destructors called when unloading the library\n");

  crazy_context_destroy(context);
  printf("OK\n");

  return 0;
}
