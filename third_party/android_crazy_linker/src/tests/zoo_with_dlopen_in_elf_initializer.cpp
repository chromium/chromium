// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdio.h>

// A version of the zoo test library that loads and unloads LIB_NAME
// in an ELF initializer / finalizer, respectively, while performing
// the dlsym() inside of Zoo().

// The point of this is to check that recursive dlopen(), are supported
// by the crazy linker.

// Name of the library that contains this code.
#ifndef THIS_LIB_NAME
#error THIS_LIB_NAME should be defined!
#endif

// Name of the library to load / unload.
#ifndef LIB_NAME
#define LIB_NAME "libcrazy_linker_tests_libzoo.so"
#endif

// Name of the entry point to find and call in the loaded library.
#ifndef LIB_SYMBOL
#define LIB_SYMBOL "Zoo"
#endif

static void* sLib = NULL;
static char sError[512] = {};

// Declare initializer and finalize functions.
static void RunOnLoad() __attribute__((constructor));
static void RunOnUnload() __attribute__((destructor));

// Define them.
void RunOnLoad() {
  printf("%s:%s: Entering\n", THIS_LIB_NAME, __FUNCTION__);
  sLib = dlopen(LIB_NAME, RTLD_NOW);
  if (!sLib) {
    fprintf(stderr, "%s:%s: Could not open %s: %s", THIS_LIB_NAME, __FUNCTION__,
            LIB_NAME, dlerror());
    return;
  }
  printf("%s:%s: Found '%s' handle %p\n", THIS_LIB_NAME, __FUNCTION__, LIB_NAME,
         sLib);

#if defined(LIB_SYMBOL)
  auto* lib_func = reinterpret_cast<void (*)()>(dlsym(sLib, LIB_SYMBOL));
  if (!lib_func) {
    fprintf(stderr, "%s:%s: Could not find '%s' symbol in %s", THIS_LIB_NAME,
            __FUNCTION__, LIB_SYMBOL, LIB_NAME);
    return;
  }
  printf("%s:%s: Found '%s' symbol at @%p\n", THIS_LIB_NAME, __FUNCTION__,
         LIB_SYMBOL, lib_func);

  // Call it.
  (*lib_func)();
#endif  // LIB_SYMBOL

  printf("%s:%s: Exiting\n", THIS_LIB_NAME, __FUNCTION__);
}

void RunOnUnload() {
  printf("%s:%s: Entering\n", THIS_LIB_NAME, __FUNCTION__);
  if (sLib) {
    int ret = dlclose(sLib);
    if (ret != 0) {
      fprintf(stderr, "%s:%s: Could not close library: %s", THIS_LIB_NAME,
              __FUNCTION__, dlerror());
    }
    sLib = NULL;
  }
  printf("%s:%s: Exiting\n", THIS_LIB_NAME, __FUNCTION__);
}

extern "C" bool Zoo() {
  printf("%s:%s:sLib=%s @ %p\n", THIS_LIB_NAME, __FUNCTION__, LIB_NAME, sLib);
  return sLib != NULL;
}
