// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that the list of valid dlopen handles managed by the crazy linker
// wrappers is maintained properly.
#include <crazy_linker.h>
#include <dlfcn.h>
#include <stdio.h>

#include "test_util.h"

namespace crazy {
extern void** GetValidDlopenHandlesForTesting(size_t*);
}  // namespace crazy

#define LIB_NAME "libcrazy_linker_tests_libbar_with_two_dlopens.so"

// The type of crazy::GetValidDlopenHandlesForTesting()
using GetValidHandlesFunction = void**(size_t*);

// The type of the Bar() function inside libbar.
using BarFunction = bool(GetValidHandlesFunction*);

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Load libzoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Bar" symbol.
  BarFunction* bar_func;
  if (!crazy_library_find_symbol(library, "Bar",
                                 reinterpret_cast<void**>(&bar_func))) {
    Panic("Could not find 'Bar' in %s\n", LIB_NAME);
  }

  // Call the 'Bar' function, passing the address of our test function
  // which is linked against the test executable, but not libbar.
  if (!(*bar_func)(&crazy::GetValidDlopenHandlesForTesting))
    return 127;

  // Close the 1st library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  crazy_context_destroy(context);
  return 0;
}
