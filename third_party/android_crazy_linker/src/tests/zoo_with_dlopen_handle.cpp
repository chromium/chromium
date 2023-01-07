// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdio.h>

using dlclose_func_t = int(void*);

extern "C" dlclose_func_t* Zoo(void* bar_lib) {
  printf("%s: Entering, library_handle=%p\n", __FUNCTION__, bar_lib);
  auto* bar_func = reinterpret_cast<void (*)()>(::dlsym(bar_lib, "Bar"));
  if (!bar_func) {
    fprintf(stderr, "ERROR: Could not find 'Bar' symbol in library\n");
    return nullptr;
  }
  printf("OK: Found 'Bar' symbol at @%p\n", bar_func);

  printf("OK: Closing library\n");
  int ret = dlclose(bar_lib);
  if (ret != 0) {
    printf("ERROR: Failed to close library: %s\n", dlerror());
    return nullptr;
  }

  dlclose_func_t* result = &::dlclose;
  printf("%s: Exiting, address of dlclose=%p\n", __FUNCTION__, result);

  // If the library is loaded with the crazy linker, this should return the
  // address of the wrapper function, not of the system dlclose one. This
  // will be checked by the caller.
  return result;
}
