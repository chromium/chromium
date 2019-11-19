// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (LIB_NAME) with the linker.
// - Find the address of the "OpenExtFindRunClose" function in LIB_NAME.
// - Call the OpenExtFindRunClose() function, which will use
// android_dlopen_ext() /
//   dlsym() to find libbar.so (which depends on libfoo.so).
// - Close the library.

// This tests the android_dlopen_ext/dlsym/dlclose wrappers provided by the
// crazy linker to loaded libraries.

#include <crazy_linker.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

#ifdef __ANDROID__
#include <android/dlext.h>
#else
#error "This source file only compiles for Android!"
#endif

#include "test_util.h"

typedef bool (*OpenExtFindRunCloseFunctionPtr)(const char* lib_name,
                                               const char* func_name,
                                               const android_dlextinfo* info);

#define LIB_NAME "libcrazy_linker_tests_libzoo_with_android_dlopen_ext.so"
#define FUNC_NAME "OpenExtFindRunClose"

#define LIB2_NAME "libcrazy_linker_tests_libbar.so"
#define FUNC2_NAME "Bar"

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Load LIB_NAME
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "OpenExtFindRunClose" symbol from LIB_NAME
  OpenExtFindRunCloseFunctionPtr lib_func;
  if (!crazy_library_find_symbol(library, "OpenExtFindRunClose",
                                 reinterpret_cast<void**>(&lib_func))) {
    Panic("Could not find '" FUNC_NAME "' in " LIB_NAME "\n");
  }

  bool ret;
  // Call it, without dlext info.
  printf("////////////////////////// FIRST CALL WITHOUT DLEXT INFO\n");
  ret = (*lib_func)(LIB2_NAME, FUNC2_NAME, nullptr);
  if (!ret)
    Panic("'" FUNC_NAME "' function failed!");

  // Call it again, this time load the library from a file descriptor.
  printf("////////////////////////// SECOND CALL WITH LIBRARY FD\n");
  {
    android_dlextinfo info = {};
    info.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    info.library_fd = ::open(LIB2_NAME, O_RDONLY | O_CLOEXEC);
    if (info.library_fd < 0)
      PanicErrno("Could not open library file directly");

    ret = (*lib_func)(nullptr, FUNC2_NAME, &info);
    if (!ret)
      Panic("'" FUNC_NAME "' function failed with file descriptor!");

    close(info.library_fd);
  }

  fflush(stdout);
  printf("////////////////////////// THIRD CALL WITH RESERVED MAP\n");
  {
    android_dlextinfo info = {};
    info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
    info.reserved_size = 64 * 4096;
    info.reserved_addr = ::mmap(nullptr, info.reserved_size, PROT_NONE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info.reserved_addr == MAP_FAILED)
      PanicErrno("Could not reserve address map region");

    ret = (*lib_func)(LIB2_NAME, FUNC2_NAME, &info);
    if (!ret)
      Panic("'" FUNC_NAME "' function failed with reserved map!");

    ::munmap(info.reserved_addr, info.reserved_size);
  }

  fflush(stdout);
  printf("////////////////////////// FOURTH CALL WITH TINY RESERVED MAP\n");
  {
    android_dlextinfo info = {};
    info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
    info.reserved_size = 1 * 4096;
    info.reserved_addr = ::mmap(nullptr, info.reserved_size, PROT_NONE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info.reserved_addr == MAP_FAILED)
      PanicErrno("Could not reserve address map region");

    ret = (*lib_func)(LIB2_NAME, FUNC2_NAME, &info);
    if (ret)
      Panic("'" FUNC_NAME "' function succeeded with tiny reserved map!");

    ::munmap(info.reserved_addr, info.reserved_size);
  }

  fflush(stdout);
  printf("////////////////////////// FIFTH CALL WITH RESERVED MAP + HINT\n");
  {
    android_dlextinfo info = {};
    info.flags =
        ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_RESERVED_ADDRESS_HINT;
    info.reserved_size = 1 * 4096;
    info.reserved_addr = ::mmap(nullptr, info.reserved_size, PROT_NONE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info.reserved_addr == MAP_FAILED)
      PanicErrno("Could not reserve address map region");

    ret = (*lib_func)(LIB2_NAME, FUNC2_NAME, &info);
    if (!ret)
      Panic("'" FUNC_NAME "' failed with tiny reserved map and hint!");

    ::munmap(info.reserved_addr, info.reserved_size);
  }

  fflush(stdout);
  printf("//////////////////////////\n");
  // Close the library.
  printf("Closing " LIB_NAME "\n");
  crazy_library_close(library);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
