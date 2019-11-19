// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SYSTEM_LINKER_H
#define CRAZY_LINKER_SYSTEM_LINKER_H

#ifdef __ANDROID__
#include <android/dlext.h>
#endif

#include <dlfcn.h>

namespace crazy {

// Convenience wrapper for the system linker functions.
// Also helps synchronize access to the global link map list.
//
// TODO(digit): Use this in the future to mock different versions/behaviours
// of the Android system linker for unit-testing purposes.
struct SystemLinker {
  // Wrapper for dlopen().
  static void* Open(const char* path, int flags);

#ifdef __ANDROID__
  // Returns true iff this system linker provides android_dlopen_ext().
  static bool HasAndroidOpenExt();

  // Calls android_dlopen_ext() if available, returns nullptr if it is not
  // available otherwise.
  static void* AndroidOpenExt(const char* path,
                              int flags,
                              const android_dlextinfo* info);
#endif  // __ANDROID__

  // Wrapper for dlclose().
  static int Close(void* handle);

  // Result type for Resolve() below.
  struct SearchResult {
    void* address = nullptr;
    void* library = nullptr;

    constexpr bool IsValid() const { return library != nullptr; }
  };

  // Wrapper for dlsym().
  static SearchResult Resolve(void* handle, const char* symbol);

  // Wrapper for dlerror().
  static const char* Error();

  // Wrapper for dladdr();
  static int AddressInfo(void* addr, Dl_info* info);
};

}  // namespace crazy

#endif  // CRAZY_LINKER_SYSTEM_LINKER_H
