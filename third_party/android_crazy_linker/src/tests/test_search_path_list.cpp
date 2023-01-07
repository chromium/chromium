// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to check that paths added with
// crazy_context_add_search_path() or
// crazy_context_add_search_path_from_address()
// have higher priority than the default ones (from LD_LIBRARY_PATH).
//
// This requires creating two temporary directories and placing there libraries
// with the same name, but different content
//
//   $TMPDIR1/libfoo.so   -> a copy of libfoo.so that contains the 'Foo' symbol.
//   $TMPDIR2/libfoo.so   -> a copy of libfoo2.so that contains the 'Foo2'
// symbol.
//

#include <crazy_linker.h>

#include "test_util.h"

namespace {

void CheckLibraryCantLoad(const char* library_name, crazy_context_t* context) {
  crazy_library_t* library;

  if (crazy_library_open(&library, library_name, context))
    Panic("Could load library %s, expected this not to be possible\n",
          library_name);
}

// Loads a library named |library_name| and checks that it contains
// the symbols listed in |wanted_symbols|, and none of the symbols
// listed in |unwanted_symbols|. After that, close the library and exit.
//
// Both |wanted_symbols| and |unwanted_symbols| are NULL-terminated
// arrays of strings.
void CheckLibrary(const char* library_name,
                  const char* const* wanted_symbols,
                  const char* const* unwanted_symbols,
                  crazy_context_t* context) {
  crazy_library_t* library;

  if (!crazy_library_open(&library, library_name, context))
    Panic("Could not open library %s: %s\n", crazy_context_get_error(context));

  size_t failures = 0;

  if (wanted_symbols) {
    for (; *wanted_symbols; ++wanted_symbols) {
      const char* symbol_name = *wanted_symbols;
      void* symbol_addr;
      if (!crazy_library_find_symbol(library, symbol_name, &symbol_addr)) {
        fprintf(stderr,
                "Could not find symbol '%s' in library '%s'!\n",
                symbol_name,
                library_name);
        failures += 1;
      }
    }
  }

  if (unwanted_symbols) {
    for (; *unwanted_symbols; ++unwanted_symbols) {
      const char* symbol_name = *unwanted_symbols;
      void* symbol_addr;
      if (crazy_library_find_symbol(library, symbol_name, &symbol_addr)) {
        fprintf(stderr,
                "Found symbol '%s' in library '%s', none expected!\n",
                symbol_name,
                library_name);
        failures += 1;
      }
    }
  }

  if (failures > 0)
    Panic("Found %d symbol failures in library %s\n", failures, library_name);

  crazy_library_close(library);
}

}  // namespace

#define LIB_NAME "libcrazy_linker_tests_libfoo.so"
#define LIB2_NAME "libcrazy_linker_tests_libfoo2.so"

int main() {
  String exe_path = GetCurrentExecutableDirectory();

  TempDirectory temp_dir_1;
  TempDirectory temp_dir_2;

  // List of symbols in original libfoo.so and libfoo2.so, respectively.
  static const char* const kFooSymbols[] = {"Foo", NULL};
  static const char* const kFoo2Symbols[] = {"Foo2", NULL};

  // Copy libfoo.so to $TMPDIR1/libfoo.so
  CopyFile(LIB_NAME, exe_path.c_str(), LIB_NAME, temp_dir_1.path());

  // Copy libfoo2.so to $TMPDIR2/libfoo.so
  CopyFile(LIB2_NAME, exe_path.c_str(), LIB_NAME, temp_dir_2.path());

  // Create a new context object.
  crazy_context_t* context = crazy_context_create();

  // Reset search paths to a non-existing directory. Check that the library
  // can't be loaded.
  setenv("LD_LIBRARY_PATH", "/this-directory-does-not-exist", 1);
  crazy_reset_search_paths();
  CheckLibraryCantLoad(LIB_NAME, context);

  // Add the search path to the current executable, this should load the
  // original
  // libfoo.so.
  crazy_add_search_path_for_address((void*)&main);
  CheckLibrary(LIB_NAME, kFooSymbols, kFoo2Symbols, context);

  // Reset search paths to use $TMPDIR2 then $TMPDIR1
  setenv("LD_LIBRARY_PATH", temp_dir_1.path(), 1);
  crazy_reset_search_paths();
  crazy_add_search_path(temp_dir_2.path());

  // Check that the copy of libfoo2.so is loaded.
  CheckLibrary(LIB_NAME, kFoo2Symbols, kFooSymbols, context);

  // Reset search paths to use only $TMPDIR1
  crazy_reset_search_paths();

  // Check that the copy of libfoo.so is loaded.
  CheckLibrary(LIB_NAME, kFooSymbols, kFoo2Symbols, context);

  crazy_context_destroy(context);

  return 0;
}
