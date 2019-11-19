// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdio.h>

// This source file is used to exercise the dlopen()/dlsym() wrappers
// inside a library that was loaded by the crazy linker. It should be compiled
// into a standalone library (e.g. libzoo.so) that is loaded with the crazy
// linker.

#ifdef __ANDROID__
#include <android/dlext.h>

// <android/dlext.h> does not declare android_dlopen_ext() if __ANDROID_API__
// is smaller than 21, so declare it here as a weak function. This will allow
// detecting its availability at runtime. For API level 21 or higher, the
// attribute is ignored due to the previous declaration.

// NOTE: The crazy linker always provides android_dlopen_ext(), even on older
//       platforms, this is required to compile this source, and will allow
//       us to check that the crazy linker resolves weak symbol imports
//       to itself properly.
extern "C" void* android_dlopen_ext(const char*, int, const android_dlextinfo*)
    __attribute__((weak));

#else
#error "This source file can only build for Android."
#endif

// Try to load library |lib_name| with android_dlopen_ext(), then locate
// function |func_name| in it to run it, then close the library.
// Return true in case of success, false otherwise.
extern "C" bool OpenExtFindRunClose(const char* lib_name,
                                    const char* func_name,
                                    const android_dlextinfo* android_info) {
  printf("%s: Entering\n", __FUNCTION__);
  if (!android_dlopen_ext) {
    fprintf(stderr, "Cannot find android_dlopen_ext symbol!");
    return false;
  }

  void* lib_handle = android_dlopen_ext(lib_name, RTLD_NOW, android_info);
  if (!lib_handle) {
    fprintf(stderr, "Could not open %s: %s\n", lib_name ? lib_name : "(NULL)",
            dlerror());
    return false;
  }
  if (!lib_name)
    lib_name = "(FROM_FD)";

  printf("%s: Opened %s @%p\n", __FUNCTION__, lib_name, lib_handle);

  auto* func_ptr = reinterpret_cast<void (*)()>(dlsym(lib_handle, func_name));
  if (!func_ptr) {
    fprintf(stderr, "Could not find 'Bar' symbol in %s: %s\n", lib_name,
            dlerror());
    return false;
  }
  printf("%s: Found '%s' symbol at @%p\n", __FUNCTION__, func_name, func_ptr);

  // Call it.
  printf("%s: Calling %s\n", __FUNCTION__, func_name);
  (*func_ptr)();

  printf("%s: Closing %s\n", __FUNCTION__, lib_name);
  int ret = dlclose(lib_handle);
  if (ret != 0) {
    printf("ERROR: Failed to close library: %s\n", dlerror());
    return false;
  }
  printf("%s: Exiting\n", __FUNCTION__);
  return true;
}
