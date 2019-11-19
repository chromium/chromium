// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_system_linker.h"

#include "crazy_linker_globals.h"

#include <dlfcn.h>

namespace crazy {

// <android/dlext.h> does not declare android_dlopen_ext() if __ANDROID_API__
// is smaller than 21, so declare it here as a weak function. This will allow
// detecting its availability at runtime. For API level 21 or higher, the
// attribute is ignored due to the previous declaration.
extern "C" void* android_dlopen_ext(const char*, int, const android_dlextinfo*)
    __attribute__((weak));

// static
void* SystemLinker::Open(const char* path, int mode) {
  // NOTE: The system linker will likely modify the global _r_debug link map
  // so ensure this doesn't conflict with other threads performing delayed
  // updates on it.
  ScopedLinkMapLocker locker;
  return ::dlopen(path, mode);
}

#ifdef __ANDROID__
// static
bool SystemLinker::HasAndroidOpenExt() {
  return android_dlopen_ext != nullptr;
}

// static
void* SystemLinker::AndroidOpenExt(const char* path,
                                   int mode,
                                   const android_dlextinfo* info) {
  // NOTE: The system linker will likely modify the global _r_debug link map
  // so ensure this doesn't conflict with other threads performing delayed
  // updates on it.
  ScopedLinkMapLocker locker;
  if (android_dlopen_ext != nullptr) {
    return android_dlopen_ext(path, mode, info);
  }
  return nullptr;
}
#endif  // __ANDROID__

// static
int SystemLinker::Close(void* handle) {
  // Similarly, though unlikely, this operation may modify the global link map.
  ScopedLinkMapLocker locker;
  return ::dlclose(handle);
}

// static
SystemLinker::SearchResult SystemLinker::Resolve(void* handle,
                                                 const char* symbol) {
  // Just in case the system linker performs lazy symbol resolution
  // that would modify the global link map.
  ScopedLinkMapLocker locker;
  void* address = ::dlsym(handle, symbol);
  if (!address) {
    // TODO(digit): Distinguish between missing symbols and weak symbols.
    return {};
  }
  return {address, handle};
}

// static
const char* SystemLinker::Error() {
  return ::dlerror();
}

int SystemLinker::AddressInfo(void* address, Dl_info* info) {
  ::dlerror();
  return ::dladdr(address, info);
}

}  // namespace crazy
