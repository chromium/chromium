// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_wrappers.h"

#include <link.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_globals.h"
#include "crazy_linker_library_list.h"
#include "crazy_linker_library_view.h"
#include "crazy_linker_shared_library.h"
#include "crazy_linker_system_linker.h"
#include "crazy_linker_thread_data.h"
#include "crazy_linker_util.h"

#ifdef __ANDROID__
#include <android/dlext.h>
#endif

#ifdef __arm__
// On ARM, this function is exported by the dynamic linker but never
// declared in any official header. It is used at runtime to
// find the base address of the .ARM.exidx section for the
// shared library containing the instruction at |pc|, as well as
// the number of 8-byte entries in that section, written into |*pcount|
extern "C" _Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr, int*);
#else
// On other architectures, this function is exported by the dynamic linker
// but never declared in any official header. It is used at runtime to
// iterate over all loaded libraries and call the |cb|. When the function
// returns non-0, the iteration returns and the function returns its
// value.
extern "C" int dl_iterate_phdr(int (*cb)(dl_phdr_info* info,
                                         size_t size,
                                         void* data),
                               void* data);
#endif

namespace crazy {

namespace {

#ifndef UNIT_TEST
// LLVM's demangler is large, and we have no need of it.  Overriding it with
// our own stub version here stops a lot of code being pulled in from libc++.
// This reduces the on-disk footprint of the crazy linker library by more than
// 70%, down from over 290kb to under 85kb on ARM.
//
// For details, see:
//   https://code.google.com/p/chromium/issues/detail?id=502299
//   https://llvm.org/svn/llvm-project/libcxxabi/trunk/src/cxa_demangle.cpp
extern "C" char* __cxa_demangle(const char* mangled_name,
                                char* buf,
                                size_t* n,
                                int* status) {
  static const int kMemoryAllocFailure = -1;  // LLVM's memory_alloc_failure.
  if (status)
    *status = kMemoryAllocFailure;
  return NULL;
}
#endif  // UNIT_TEST

#ifdef __arm__
extern "C" int __cxa_atexit(void (*)(void*), void*, void*);

// On ARM, this function is defined as a weak symbol by libc.so.
// Unfortunately its address cannot be found through dlsym(), which will
// always return NULL. To work-around this, define a copy here that does
// exactly the same thing. The ARM EABI mandates the function's behaviour.
// __cxa_atexit() is implemented by the C library, but not declared by
// any official header. It's part of the low-level C++ support runtime.
int __aeabi_atexit(void* object, void (*destructor)(void*), void* dso_handle) {
  return __cxa_atexit(destructor, object, dso_handle);
}
#endif

// Used to save the system dlerror() into our thread-specific data.
void SaveSystemError() {
  ThreadData* data = GetThreadData();
  data->SetError(SystemLinker::Error());
}

char* WrapDlerror() {
  ThreadData* data = GetThreadData();
  const char* error = data->GetError();
  data->SwapErrorBuffers();
  // dlerror() returns a 'char*', but no sane client code should ever
  // try to write to this location.
  return const_cast<char*>(error);
}

void* WrapDlopen(const char* path, int mode) {
  ScopedLockedGlobals globals;
  LibraryList* libs = globals->libraries();

  // NOTE: If |path| is NULL, the wrapper should return a handle
  // corresponding to the current executable. This can't be a crazy
  // library, so don't try to handle it with the crazy linker.
  if (path) {
    LibraryView* view = libs->FindKnownLibrary(path);
    if (!view) {
      Error error;
      LoadParams params;
      if (libs->LocateLibraryFile(path, *globals->search_path_list(), &params,
                                  &error)) {
        view = libs->LoadLibraryInternal(params, &error);
        if (!view) {
          SetLinkerError("%s: %s", "dlopen", error.c_str());
          return nullptr;
        }
        globals->valid_handles()->Add(view);
        return view;
      }
    }
  }

  // Try to load the executable with the system dlopen() instead.
  void* system_lib = SystemLinker::Open(path, mode);
  if (system_lib == NULL) {
    SaveSystemError();
    return nullptr;
  }

  auto* view = new LibraryView(system_lib, path ? path : "<executable>");
  libs->AddLibrary(view);
  globals->valid_handles()->Add(view);
  return view;
}

#ifdef __ANDROID__
// Prepare LoadParams according to |path| and |info|.
static LoadParams PrepareLoadParamsFrom(const char* path,
                                        const android_dlextinfo* info) {
  LoadParams params;
  if (info->flags & ANDROID_DLEXT_USE_LIBRARY_FD) {
    params.library_fd = info->library_fd;
    if (info->flags & ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET)
      params.library_offset = info->library_fd_offset;
  } else {
    params.library_path = path;
  }
  if (info->flags & ANDROID_DLEXT_RESERVED_ADDRESS) {
    params.wanted_address = reinterpret_cast<uintptr_t>(info->reserved_addr);
    params.reserved_size = info->reserved_size;
  }
  if (info->flags & ANDROID_DLEXT_RESERVED_ADDRESS_HINT)
    params.reserved_load_fallback = true;

  return params;
}

void* WrapAndroidDlopenExt(const char* path,
                           int mode,
                           const android_dlextinfo* info) {
  if (!info)
    return WrapDlopen(path, mode);

  ScopedLockedGlobals globals;

  const uint64_t kSupportedFlags =
      ANDROID_DLEXT_USE_LIBRARY_FD | ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET |
      ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_RESERVED_ADDRESS_HINT;
  const uint64_t unsupported_flags = (info->flags & ~kSupportedFlags);
  if (unsupported_flags) {
    SetLinkerError("%s", "unsupported android_dlextinfo flags %08llx",
                   "android_dlopen_ext",
                   static_cast<unsigned long long>(unsupported_flags));
    return nullptr;
  }

  if (!path && !(info->flags & ANDROID_DLEXT_USE_LIBRARY_FD)) {
    SetLinkerError("%s: missing path or file descriptor.",
                   "android_dlopen_ext");
    return nullptr;
  }
  Error error;
  LibraryView* view = globals->libraries()->LoadLibraryInternal(
      PrepareLoadParamsFrom(path, info), &error);
  if (!view) {
    SetLinkerError("%s: %s", "android_dlopen_ext", error.c_str());
    return nullptr;
  }
  globals->valid_handles()->Add(view);
  return view;
}
#endif  // __ANDROID__

void* WrapDlsym(void* lib_handle, const char* symbol_name) {
  if (!symbol_name) {
    SetLinkerError("dlsym: NULL symbol name");
    return NULL;
  }

  if (!lib_handle) {
    SetLinkerError("dlsym: NULL library handle");
    return NULL;
  }

  // TODO(digit): Handle RTLD_DEFAULT / RTLD_NEXT
  if (lib_handle == RTLD_DEFAULT || lib_handle == RTLD_NEXT) {
    SetLinkerError("dlsym: RTLD_DEFAULT/RTLD_NEXT are not implemented!");
    return NULL;
  }

  // NOTE: The Android dlsym() only looks inside the target library,
  // while the GNU one will perform a breadth-first search into its
  // dependency tree.

  // This implementation performs a correct breadth-first search
  // when |lib_handle| corresponds to a crazy library, except that
  // it stops at system libraries that it depends on.

  ScopedLockedGlobals globals;
  if (!globals->valid_handles()->Has(lib_handle)) {
    // Note: the handle was not opened with the crazy linker, so fall back
    // to the system linker. That can happen in rare cases.
    SystemLinker::SearchResult sym =
        SystemLinker::Resolve(lib_handle, symbol_name);
    if (!sym.IsValid()) {
      SaveSystemError();
      LOG("Could not find symbol '%s' from foreign library [%s]", symbol_name,
          GetThreadData()->GetError());
    }
    return sym.address;
  }

  auto* wrap_lib = reinterpret_cast<LibraryView*>(lib_handle);
  if (wrap_lib->IsSystem()) {
    LibraryView::SearchResult sym = wrap_lib->LookupSymbol(symbol_name);
    if (!sym.IsValid()) {
      SaveSystemError();
      LOG("Could not find symbol '%s' from system library %s [%s]", symbol_name,
          wrap_lib->GetName(), GetThreadData()->GetError());
    }
    return sym.address;
  }

  if (wrap_lib->IsCrazy()) {
    void* addr = globals->libraries()->FindSymbolFrom(symbol_name, wrap_lib);
    if (addr)
      return addr;

    SetLinkerError("dlsym: Could not find '%s' from library '%s'",
                   symbol_name,
                   wrap_lib->GetName());
    return NULL;
  }

  SetLinkerError("dlsym: Invalid library handle %p looking for '%s'",
                 lib_handle,
                 symbol_name);
  return NULL;
}

int WrapDladdr(void* address, Dl_info* info) {
  // First, perform search in crazy libraries.
  {
    ScopedLockedGlobals globals;
    const LibraryView* wrap =
        globals->libraries()->FindLibraryForAddress(address);
    if (wrap && wrap->IsCrazy()) {
      size_t sym_size = 0;

      const SharedLibrary* lib = wrap->GetCrazy();
      ::memset(info, 0, sizeof(*info));
      info->dli_fname = lib->base_name();
      info->dli_fbase = reinterpret_cast<void*>(lib->load_address());

      // Determine if any symbol in the library contains the specified address.
      (void)lib->FindNearestSymbolForAddress(
          address, &info->dli_sname, &info->dli_saddr, &sym_size);
      return 0;
    }
  }
  // Otherwise, use system version.
  int ret = SystemLinker::AddressInfo(address, info);
  if (ret != 0)
    SaveSystemError();
  return ret;
}

int WrapDlclose(void* lib_handle) {
  if (!lib_handle) {
    SetLinkerError("NULL library handle");
    return -1;
  }

  ScopedLockedGlobals globals;
  if (!globals->valid_handles()->Remove(lib_handle)) {
    // This is a foreign handle that was not created by the crazy linker.
    // Fall-back to the system in this case.
    if (SystemLinker::Close(lib_handle) != 0) {
      SaveSystemError();
      LOG("Could not close foreign library handle %p\n%s", lib_handle,
          GetThreadData()->GetError());
      return -1;
    }
    return 0;
  }

  LibraryView* wrap_lib = reinterpret_cast<LibraryView*>(lib_handle);
  if (wrap_lib->IsSystem() || wrap_lib->IsCrazy()) {
    globals->libraries()->UnloadLibrary(wrap_lib);
    return 0;
  }

  // Invalid library handle!!
  SetLinkerError("Invalid library handle %p", lib_handle);
  return -1;
}

#ifdef __arm__
_Unwind_Ptr WrapDl_unwind_find_exidx(_Unwind_Ptr pc, int* pcount) {
  // First lookup in crazy libraries.
  {
    ScopedLockedGlobals globals;
    _Unwind_Ptr result =
        globals->libraries()->FindArmExIdx(reinterpret_cast<void*>(pc), pcount);
    if (result)
      return result;
  }
  // Lookup in system libraries.
  return ::dl_unwind_find_exidx(pc, pcount);
}
#else  // !__arm__
int WrapDl_iterate_phdr(int (*cb)(dl_phdr_info*, size_t, void*), void* data) {
  // First, iterate over crazy libraries.
  {
    ScopedLockedGlobals globals;
    int result = globals->libraries()->IteratePhdr(cb, data);
    if (result)
      return result;
  }
  // Then lookup through system ones.
  return ::dl_iterate_phdr(cb, data);
}
#endif  // !__arm__

}  // namespace

// This method should only be called from testing code. It is used by
// one integration test to check that wrapping works correctly within
// libraries loaded through the crazy-linker.
void* GetDlCloseWrapperAddressForTesting() {
  return reinterpret_cast<void*>(&WrapDlclose);
}

// This method should only be called from testing code. It is used to return
// the list of valid dlopen() handles created by the crazy-linker. This returns
// the address of a heap-allocated array of pointers, which must be explicitly
// free()-ed by the caller. This returns nullptr is the array is empty.
// On exit, sets |*p_count| to the number of items in the array.
const void** GetValidDlopenHandlesForTesting(size_t* p_count) {
  ScopedLockedGlobals globals;
  const Vector<const void*>& handles =
      globals->valid_handles()->GetValuesForTesting();
  *p_count = handles.GetCount();
  if (handles.IsEmpty())
    return nullptr;

  auto* ptr = reinterpret_cast<const void**>(
      malloc(handles.GetCount() * sizeof(void*)));
  for (size_t n = 0; n < handles.GetCount(); ++n) {
    ptr[n] = handles[n];
  }
  return ptr;
}

void* WrapLinkerSymbol(const char* name) {
  // Shortcut, since all names begin with 'dl'
  // Take care of __aeabi_atexit on ARM though.
  if (name[0] != 'd' || name[1] != 'l') {
#ifdef __arm__
    if (name[0] == '_' && !strcmp("__aeabi_atexit", name))
      return reinterpret_cast<void*>(&__aeabi_atexit);
#endif
#ifdef __ANDROID__
    if (name[0] == 'a' && !strcmp("android_dlopen_ext", name))
      return reinterpret_cast<void*>(&WrapAndroidDlopenExt);
#endif
    return NULL;
  }

  static const struct {
    const char* name;
    void* address;
  } kSymbols[] = {
        {"dlopen", reinterpret_cast<void*>(&WrapDlopen)},
        {"dlclose", reinterpret_cast<void*>(&WrapDlclose)},
        {"dlerror", reinterpret_cast<void*>(&WrapDlerror)},
        {"dlsym", reinterpret_cast<void*>(&WrapDlsym)},
        {"dladdr", reinterpret_cast<void*>(&WrapDladdr)},
#ifdef __arm__
        {"dl_unwind_find_exidx",
         reinterpret_cast<void*>(&WrapDl_unwind_find_exidx)},
#else
        {"dl_iterate_phdr", reinterpret_cast<void*>(&WrapDl_iterate_phdr)},
#endif
    };
  static const size_t kCount = sizeof(kSymbols) / sizeof(kSymbols[0]);
  for (size_t n = 0; n < kCount; ++n) {
    if (!strcmp(kSymbols[n].name, name))
      return kSymbols[n].address;
  }
  return NULL;
}

}  // namespace crazy
