// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SHARED_LIBRARY_H
#define CRAZY_LINKER_SHARED_LIBRARY_H

#include <link.h>

#include <utility>

#include "crazy_linker_elf_relro.h"
#include "crazy_linker_elf_symbols.h"
#include "crazy_linker_elf_view.h"
#include "crazy_linker_error.h"
#include "crazy_linker_load_params.h"
#include "crazy_linker_memory_mapping.h"
#include "crazy_linker_rdebug.h"
#include "crazy_linker_util.h"
#include "elf_traits.h"

namespace crazy {

class LibraryList;
class LibraryView;

// A class that models a shared library loaded by the crazy linker.

// Libraries have dependencies (which are listed in their dynamic section
// as DT_NEEDED entries). Circular dependencies are forbidden, so they
// form an ADG, where the root is the crazy linker itself, since all
// libraries that it loads will depend on it (to ensure their
// dlopen/dlsym/dlclose calls are properly wrapped).

class SharedLibrary {
 public:
  SharedLibrary();
  ~SharedLibrary();

  size_t load_address() const { return view_.load_address(); }
  size_t load_size() const { return view_.load_size(); }
  size_t load_bias() const { return view_.load_bias(); }
  const ELF::Phdr* phdr() const { return view_.phdr(); }
  size_t phdr_count() const { return view_.phdr_count(); }
  const char* base_name() const { return base_name_; }

  // Return name of the library as found in DT_SONAME entry, or same
  // as base_name() if not available.
  const char* soname() const { return soname_; }

  // Load a library (without its dependents) from an ELF file.
  // Note: This does not apply relocations, nor runs constructors.
  // |full_path| if the file full path.
  // |params| are the load parameters for this operation.
  // On failure, return false and set |error| message.
  //
  // After this, the caller should load all library dependencies,
  // Then call Relocate() and CallConstructors() to complete the
  // operation.
  bool Load(const LoadParams& params, Error* error);

  // Relocate this library, assuming all its dependencies are already
  // loaded in |lib_list|. On failure, return false and set |error|
  // message.
  bool Relocate(LibraryList* lib_list,
                const Vector<LibraryView*>* preloads,
                const Vector<LibraryView*>* dependencies,
                Error* error);

  void GetInfo(size_t* load_address,
               size_t* load_size,
               size_t* relro_start,
               size_t* relro_size) {
    *load_address = view_.load_address();
    *load_size = view_.load_size();
    *relro_start = relro_start_;
    *relro_size = relro_size_;
  }

  // Returns true iff a given library is mapped to a virtual address range
  // that contains a given address.
  bool ContainsAddress(void* address) const {
    size_t addr = reinterpret_cast<size_t>(address);
    return load_address() <= addr && addr <= load_address() + load_size();
  }

  // Call all constructors in the library.
  void CallConstructors();

  // Call all destructors in the library.
  void CallDestructors();

  // Return the ELF symbol entry for a given symbol, if defined by
  // this library, or NULL otherwise.
  const ELF::Sym* LookupSymbolEntry(const char* symbol_name) const;

  // Find the nearest symbol near a given |address|. On success, return
  // true and set |*sym_name| to the symbol name, |*sym_addr| to its address
  // in memory, and |*sym_size| to its size in bytes, if any.
  bool FindNearestSymbolForAddress(void* address,
                                   const char** sym_name,
                                   void** sym_addr,
                                   size_t* sym_size) const {
    return symbols_.LookupNearestByAddress(
        address, load_bias(), sym_name, sym_addr, sym_size);
  }

  // Return the address of a given |symbol_name| if it is exported
  // by the library, NULL otherwise.
  void* FindAddressForSymbol(const char* symbol_name) const;

  // Create a new Ashmem region holding a copy of the library's RELRO section,
  // potentially relocated for a new |load_address|. On success, return true
  // and sets |*relro_start|, |*relro_size| and |*relro_fd|. Note that the
  // RELRO start address is adjusted for |load_address|, and that the caller
  // becomes the owner of |*relro_fd|. On failure, return false and set
  // |error| message.
  bool CreateSharedRelro(size_t load_address,
                         size_t* relro_start,
                         size_t* relro_size,
                         int* relro_fd,
                         Error* error);

  // Try to use a shared relro section from another process.
  // On success, return true. On failure return false and
  // sets |error| message.
  bool UseSharedRelro(size_t relro_start,
                      size_t relro_size,
                      int relro_fd,
                      Error* error);

  // Look for a symbol named 'JNI_OnLoad' in this library, and if it
  // exists, call it with |java_vm| as the first parameter. If the
  // function result is less than |minimum_jni_version|, fail with
  // a message in |error|. On success, return true, and record
  // |java_vm| to call 'JNI_OnUnload' at unload time, if present.
  bool CallJniOnLoad(void* java_vm, int minimum_jni_version, Error* error);

  // Call 'JNI_OnUnload()' is necessary, i.e. if there was a succesful call
  // to CallJniOnLoad() before, or nothing otherwise.
  void CallJniOnUnload();

  // Release reserved memory mapping. Caller takes ownership. Used to delay
  // the unmapping of the library segments in the case of delayed RDebug
  // operations.
  MemoryMapping ReleaseMapping() { return std::move(reserved_map_); }

  // Helper class to iterate over dependencies in a given SharedLibrary.
  // Usage:
  //    SharedLibary::DependencyIterator iter(lib);
  //    while (iter.GetNext() {
  //      dependency_name = iter.GetName();
  //      ...
  //    }
  class DependencyIterator {
   public:
    explicit DependencyIterator(const SharedLibrary* lib)
        : iter_(&lib->view_), symbols_(&lib->symbols_), dep_name_(NULL) {}

    bool GetNext();

    const char* GetName() const { return dep_name_; }

   private:
    DependencyIterator() = delete;
    DependencyIterator(const DependencyIterator&) = delete;
    DependencyIterator& operator=(const DependencyIterator&) = delete;

    ElfView::DynamicIterator iter_;
    const ElfSymbols* symbols_;
    const char* dep_name_;
  };

  typedef void (*linker_function_t)();

 private:
  friend class LibraryList;

  ElfView view_;
  ElfSymbols symbols_;
  MemoryMapping reserved_map_;

  ELF::Addr relro_start_ = 0;
  ELF::Addr relro_size_ = 0;
  bool relro_used_ = false;

  SharedLibrary* list_next_ = nullptr;
  SharedLibrary* list_prev_ = nullptr;
  unsigned flags_ = 0;

  linker_function_t* preinit_array_ = nullptr;
  size_t preinit_array_count_ = 0;
  linker_function_t* init_array_ = nullptr;
  size_t init_array_count_ = 0;
  linker_function_t* fini_array_ = nullptr;
  size_t fini_array_count_ = 0;
  linker_function_t init_func_ = nullptr;
  linker_function_t fini_func_ = nullptr;
#ifdef __arm__
  // ARM EABI section used for stack unwinding.
  unsigned* arm_exidx_ = nullptr;
  size_t arm_exidx_count_ = 0;
#endif

  link_map_t link_map_ = {};

  bool has_DT_SYMBOLIC_ = false;

  void* java_vm_ = nullptr;

  const char* soname_ = nullptr;
  const char* base_name_ = nullptr;
  char full_path_[512];
};

}  // namespace crazy

#endif  // CRAZY_LINKER_SHARED_LIBRARY_H
