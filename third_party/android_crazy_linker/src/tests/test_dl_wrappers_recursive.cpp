// Copyright 2018 The Chromium Authors
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

#include <crazy_linker.h>
#include <stdio.h>

#include "test_util.h"

typedef bool (*FunctionPtr)();

#define LIB_NAME "libcrazy_linker_tests_libzoo_dlopen_in_initializer.so"

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Load library. Its ELF initializer will call dlopen() to open another
  // library, the use dlsym() to find and invoke a function inside it.
  //
  // Said function will actually also call dlopen() to load another library
  // and invoke another function in another library, i.e.:
  //
  //  exe:     crazy_library_open("libA.so")
  //  linker:    load and relocate libA.so
  //  linker:    call ELF initializers in libA.so
  //  libA.so:     call dlopen("libB.so")
  //  linker:        crazy::WrapDlOpen("libB.so") gets called.
  //  linker:          load and relocate libB.so
  //  linker:          run ELF initializer in libB.so
  //  libB.so:           call dlopen() to load library C
  //  linker:              crazy::WrapDlOpen("libC.so") gets called.
  //  linker:                load and relocate libC.so
  //  libB.so:           call dlsym() to locate C entry point
  //  libB.so:           call C entry point.
  //  libC.so:             call dlopen() to load library D
  //  linker:                crazy::WrapDlOpen("libD.so") gets called.
  //  linker:                  load and relocate libD.so
  //  libC.so:             call dlsym() to locate D entry point.
  //  libC.so:             call D entry point
  //  libD.so:               execute function and return.
  //  libC.so:             call dlclose() to unload libD.so.
  //  libC.so:             return from function
  //  libB.so:           return from initializer
  //  linker:          libB.so has finished loading.
  //  libA.so:      call dlsym() to locate B entry point.
  //  libA.so:      call B entry point.
  //  libB.so:        execute function and return
  //  libA.so:      call dlclose() to unload libB.so
  //  libA.so:      return from initializer.
  //  linker:     libA.so has finished loading.
  //  exe:      crazy_library_find_symbol() to find A entry point
  //  libA.so:    execute A function and return.
  //  exe:      crazy_library_close() to unload libA.so
  //  linker:     call ELF finalizers in libA.so
  //  libA.so:      call dlclose() to unload libB.so
  //  linker:         crazy::WrapDlClose() gets called.
  //  linker:           run ELF finalizers in libB.so
  //  libB.so:            call dlclose() to unload libC.so.
  //  linker:               crazy::WrapDlClose() gets called.
  //  linker:                 unload libC.so (no finalizers).
  //  libB.so:          return from finalizer.
  //  linker:         libB.so has finished unloading.
  //  libA.so:      return from finalizer.
  //  linker:     libA.so has finished unloaded.
  //  exe:      program continues and exits.
  //
  // Actual names for this test:
  //  libA.so: libcrazy_linker_tests_libzoo_dlopen_in_initializer.so
  //  libB.so: libcrazy_linker_tests_libzoo_dlopen_in_initializer_inner.so
  //  libC.so: libcrazy_linker_tests_libzoo.so
  //  libD.so: libcrazy_linker_tests_libbar.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Zoo" symbol.
  FunctionPtr zoo_func;
  if (!crazy_library_find_symbol(library, "Zoo",
                                 reinterpret_cast<void**>(&zoo_func))) {
    Panic("Could not find 'Zoo' in " LIB_NAME "\n");
  }

  // Call it
  (*zoo_func)();

  // Close the library.
  printf("Closing " LIB_NAME "\n");
  crazy_library_close(library);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
