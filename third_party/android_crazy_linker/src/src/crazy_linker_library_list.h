// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LIBRARY_LIST_H
#define CRAZY_LINKER_LIBRARY_LIST_H

#include <link.h>

#include "crazy_linker_error.h"
#include "crazy_linker_expected.h"
#include "crazy_linker_load_params.h"
#include "crazy_linker_search_path_list.h"
#include "elf_traits.h"

// This header contains definitions related to the global list of
// library views maintained by the crazy linker. Each library view
// points to either a crazy library, or a system one.

namespace crazy {

class SharedLibrary;
class LibraryView;

// The list of all shared libraries loaded by the crazy linker.
// IMPORTANT: This class is not thread-safe!
class LibraryList {
 public:
  LibraryList();
  ~LibraryList();

  // Find a library in the list by its base name.
  // |base_name| must not contain a directory separator.
  LibraryView* FindLibraryByName(const char* base_name);

  // Lookup for a given |symbol_name|, starting from |from_lib|
  // then through its dependencies in breadth-first search order.
  // On failure, returns NULL.
  void* FindSymbolFrom(const char* symbol_name, const LibraryView* from_lib);

  // Return the address of a visible given symbol. Used to implement
  // the dlsym() wrapper. Returns NULL on failure.
  void* FindAddressForSymbol(const char* symbol_name);

  // Find a SharedLibrary that contains a given address, or NULL if none
  // could be found. This simply scans all libraries.
  LibraryView* FindLibraryForAddress(void* address);

#ifdef __arm__
  // Find the base address of the .ARM.exidx section corresponding
  // to the address |pc|, as well as the number of 8-byte entries in
  // the table into |*count|. Used to implement the wrapper for
  // dl_unwind_find_exidx().
  _Unwind_Ptr FindArmExIdx(void* pc, int* count);
#else
  typedef int (*PhdrIterationCallback)(dl_phdr_info* info,
                                       size_t info_size,
                                       void* data);

  // Loop over all loaded libraries and call the |cb| callback
  // on each iteration. If the function returns 0, stop immediately
  // and return its value. Used to implement the wrapper for
  // dl_iterate_phdr().
  int IteratePhdr(PhdrIterationCallback callback, void* data);
#endif

  // Find whether a library identified by |name| has already been loaded.
  // Note that |name| should correspond to the library's unique soname, which
  // comes from its DT_SONAME entry, and typically, but not necessarily
  // matches its base name.
  LibraryView* FindKnownLibrary(const char* name);

  // Check whether |lib_path| matches an already loaded library, compatible
  // with the content of |load_params| (except its |library_path| field).
  // On failure, i.e. if the load parameters are incompatible, set |*error|
  // and return its address. On success, return either nullptr (if the library
  // was not previously loaded, or a LibraryView* pointer after incrementing
  // its reference count).
  Expected<LibraryView*> FindAndCheckLoadedLibrary(
      const char* lib_path,
      const LoadParams& load_params,
      Error* error);

  // Locate library |lib_name| using |search_path_list|. On success, update
  // |params->library_path| and |params->library_offset| and return true. On
  // failure, set |*error| and return false.
  static bool LocateLibraryFile(const char* lib_name,
                                const SearchPathList& search_path_list,
                                LoadParams* params,
                                Error* error);

  // Try to load a library, according to |load_params|. On failure, returns
  // nullptr and sets the |error| message.
  LibraryView* LoadLibrary(const char* lib_name,
                           const LoadParams& load_params,
                           Error* error);

  // Try to load a library, according to |load_params|.
  // On failure, return nullptr and sets the |error| message.
  // Note: this will fail if the library is already loaded.
  LibraryView* LoadLibraryInternal(const LoadParams& load_params, Error* error);

  // Unload a given shared library. This really decrements the library's
  // internal reference count. When it reaches zero, the library's
  // destructors are run, its dependencies are unloaded, then the
  // library is removed from memory.
  void UnloadLibrary(LibraryView* lib);

  // Used internally by the wrappers only.
  void AddLibrary(LibraryView* lib);

 private:
  LibraryList(const LibraryList&);
  LibraryList& operator=(const LibraryList&);

  // Preload any libraries that override symbols in later loads.
  // Called once only, on library list construction. Libraries to preload
  // are controlled by LD_PRELOAD.
  void LoadPreloads();

  // Load a library with the system linker and a specific set of flags.
  // |lib_name| is the library name or path, |dlopen_flags| should be
  // a set of RTLD_XXX flags. On success, return a new LibraryView pointer.
  // On failure, set |*error| then return nullptr.
  LibraryView* LoadLibraryWithSystemLinker(const char* lib_name,
                                           int dlopen_flags,
                                           Error* error);

  void ClearError();

  // The list of all preloaded libraries.
  Vector<LibraryView*> preloaded_libraries_;

  // The list of all known libraries.
  Vector<LibraryView*> known_libraries_;

  // The list of all libraries loaded by the crazy linker.
  // This does _not_ include system libraries present in known_libraries_.
  SharedLibrary* head_ = nullptr;

  bool has_error_ = false;
  char error_buffer_[512];
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LIBRARY_LIST_H
