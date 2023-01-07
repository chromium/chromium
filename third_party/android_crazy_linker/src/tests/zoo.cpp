// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdio.h>

#define LIB_NAME "libcrazy_linker_tests_libbar.so"

extern "C" bool Zoo() {
  printf("%s: Entering\n", __FUNCTION__);
  void* bar_lib = dlopen(LIB_NAME, RTLD_NOW);
  if (!bar_lib) {
    fprintf(stderr, "Could not open %s: %s\n", LIB_NAME, dlerror());
    return false;
  }
  printf("%s: Opened %s @%p\n", __FUNCTION__, LIB_NAME, bar_lib);

  void (*bar_func)();

  bar_func = reinterpret_cast<void (*)()>(dlsym(bar_lib, "Bar"));
  if (!bar_func) {
    fprintf(stderr, "Could not find 'Bar' symbol in %s\n", LIB_NAME);
    return false;
  }
  printf("%s: Found 'Bar' symbol at @%p\n", __FUNCTION__, bar_func);

  // Call it.
  printf("%s: Calling Bar()\n", __FUNCTION__);
  (*bar_func)();

  printf("%s: Closing %s\n", __FUNCTION__, LIB_NAME);
  dlclose(bar_lib);

  printf("%s: Exiting\n", __FUNCTION__);
  return true;
}
