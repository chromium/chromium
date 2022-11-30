// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libzoo) with the crazy linker.
// - Find the address of the "Zoo" function in libzoo.
// - Load a library (libbar) with the *system* linker.
// - Call the Zoo() function, passing the handle for libbar to it.
//
//   Zoo() will call the crazy wrapper for dlsym() with a system handle.
//   The wrapper is supposed to recognize the handle as not one of its
//   own and fall-back to the system dlsym().
//
//   This will allow the function to find "Bar" within libbar()
//   then call it. After that, Zoo() wll call the crazy wrapper for
//   dlclose() with the same handle. The wrapper should recognize
//   this and close bar as well.
//
//   Finally, Zoo() will return the address of dlclose() as it
//   sees it, which should be the address of the crazy wrapper for
//   it, as checked by this program at the end of main().

#include <crazy_linker.h>
#include <dlfcn.h>
#include <stdio.h>

#include "test_util.h"

using dlclose_func_t = int(void*);
using zoo_func_t = dlclose_func_t*(void*);

namespace crazy {
extern void* GetDlCloseWrapperAddressForTesting();
}  // namespace crazy

#define LIB_NAME "libcrazy_linker_tests_libzoo_with_dlopen_handle.so"
#define LIB2_NAME "libcrazy_linker_tests_libbar.so"

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Load libzoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Zoo" symbol.
  zoo_func_t* zoo_func;
  if (!crazy_library_find_symbol(library, "Zoo",
                                 reinterpret_cast<void**>(&zoo_func))) {
    Panic("Could not find 'Zoo' in %s\n", LIB_NAME);
  }
  printf("OK: Found 'Zoo' at %p in %s\n", zoo_func, LIB_NAME);

  // Open the 2nd library with the system linker.
  void* bar_lib = ::dlopen(LIB2_NAME, RTLD_NOW);
  if (!bar_lib) {
    Panic("Could not open %s: %s\n", LIB2_NAME, dlerror());
  }

  printf("OK: Loaded libbar @%p with system linker\n", bar_lib);

  // Call Zoo(), passing the system handle to the second library.
  dlclose_func_t* ret = (*zoo_func)(bar_lib);
  if (!ret)
    Panic("'Zoo' function failed (returned nullptr)!\n");

  auto* expected_ret = reinterpret_cast<dlclose_func_t*>(
      crazy::GetDlCloseWrapperAddressForTesting());

  if (ret == &::dlclose) {
    Panic("'Zoo' returned system dlclose() @%p, expected wrapper @%p\n",
          &::dlclose, expected_ret);
  }
  if (ret != expected_ret) {
    Panic("'Zoo' returned unknown address %p, expected wrapper @%p\n", ret,
          expected_ret);
  }

  printf("OK: Returned address was dlclose wrapper\n");

  // Close the 1st library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  crazy_context_destroy(context);

  return 0;
}
