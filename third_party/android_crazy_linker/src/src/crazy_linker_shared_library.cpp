// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_shared_library.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <elf.h>

#include "crazy_linker_ashmem.h"
#include "crazy_linker_debug.h"
#include "crazy_linker_elf_loader.h"
#include "crazy_linker_elf_relocations.h"
#include "crazy_linker_globals.h"
#include "crazy_linker_library_list.h"
#include "crazy_linker_library_view.h"
#include "crazy_linker_load_params.h"
#include "crazy_linker_memory_mapping.h"
#include "crazy_linker_system_linker.h"
#include "crazy_linker_thread_data.h"
#include "crazy_linker_util.h"
#include "crazy_linker_wrappers.h"
#include "linker_phdr.h"

#ifndef DF_SYMBOLIC
#define DF_SYMBOLIC 2
#endif

#ifndef DF_TEXTREL
#define DF_TEXTREL 4
#endif

#ifndef DT_INIT_ARRAY
#define DT_INIT_ARRAY 25
#endif

#ifndef DT_INIT_ARRAYSZ
#define DT_INIT_ARRAYSZ 27
#endif

#ifndef DT_FINI_ARRAY
#define DT_FINI_ARRAY 26
#endif

#ifndef DT_FINI_ARRAYSZ
#define DT_FINI_ARRAYSZ 28
#endif

#ifndef DT_FLAGS
#define DT_FLAGS 30
#endif

#ifndef DT_PREINIT_ARRAY
#define DT_PREINIT_ARRAY 32
#endif

#ifndef DT_PREINIT_ARRAYSZ
#define DT_PREINIT_ARRAYSZ 33
#endif

namespace crazy {

namespace {

int local_isnanf(float x) {
  uint32_t bits;
  memcpy(&bits, &x, sizeof bits);
  if ((bits & 0x7f800000) != 0x7f800000)
    return 0;
  return (bits & 0x7fffff) ? 1 : 0;
}

typedef SharedLibrary::linker_function_t linker_function_t;
typedef int (*JNI_OnLoadFunctionPtr)(void* vm, void* reserved);
typedef void (*JNI_OnUnloadFunctionPtr)(void* vm, void* reserved);

// Call a constructor or destructor function pointer. Ignore
// NULL and -1 values intentionally. They correspond to markers
// in the tables, or deleted values.
// |func_type| corresponds to the type of the function, and is only
// used for debugging (examples are "DT_INIT", "DT_INIT_ARRAY", etc...).
void CallFunction(linker_function_t func, const char* func_type) {
  uintptr_t func_address = reinterpret_cast<uintptr_t>(func);

  LOG("%p %s", func, func_type);
  if (func_address != 0 && func_address != uintptr_t(-1))
    func();
}

// An instance of ElfRelocations::SymbolResolver that can be used
// to resolve symbols in a shared library being loaded by
// LibraryList::LoadLibrary.
class SharedLibraryResolver : public ElfRelocations::SymbolResolver {
 public:
  SharedLibraryResolver(SharedLibrary* lib,
                        LibraryList* lib_list,
                        const Vector<LibraryView*>* preloads,
                        const Vector<LibraryView*>* dependencies)
      : main_program_handle_(SystemLinker::Open(NULL, RTLD_NOW)),
        lib_(lib),
        preloads_(preloads),
        dependencies_(dependencies) {}

  ~SharedLibraryResolver() { SystemLinker::Close(main_program_handle_); }

  virtual void* Lookup(const char* symbol_name) {
    // IMPORTANT NOTE: This code is completely buggy, when relocating symbol
    // a correct ELF linker should only consider libraries in the global scope,
    // or other libraries in the same load group.
    //
    // The global scope is defined as the program executable, any of its
    // direct dependencies, any preloads, as well as any library loaded later
    // with the RTLD_GLOBAL flag.
    //
    // Normally, one can lookup symbols in it using dlsym(RTLD_DEFAULT, <name>)
    // or using dlsym() with a handle created with dlopen(NULL, ...). However
    // the Android system linker didn't always implement these cases properly.

    // TODO(digit): Fix this by totally changing the way libraries are loaded
    // and relocated, and provide mock SystemLinker implementations that
    // mimic the broken implementations of the Android linker for proper
    // testing.

    // First, look inside the current library.
    const ELF::Sym* entry = lib_->LookupSymbolEntry(symbol_name);
    if (entry)
      return reinterpret_cast<void*>(lib_->load_bias() + entry->st_value);

    // Special case: redirect the dynamic linker symbols to our wrappers.
    // This ensures that loaded libraries can call dlopen() / dlsym()
    // and transparently use the crazy linker to perform their duty.
    void* address = WrapLinkerSymbol(symbol_name);
    if (address)
      return address;

    // Then look inside the preloads.
    //
    // Note that searching preloads *before* the main executable is opposite
    // to the search ordering used by the system linker, but it is required
    // to work round a dlsym() bug in some Android releases (on releases
    // without this dlsym() bug preloads_ will be empty, making this preloads
    // search a no-op).
    //
    // For more, see commentary in LibraryList(), and
    //   https://code.google.com/p/android/issues/detail?id=74255
    for (const LibraryView* preload : *preloads_) {
      // LOG("Looking into preload %p (%s)", wrap,
      // wrap->GetName());
      address = LookupIn(symbol_name, preload);
      if (address)
        return address;
    }

    // Then lookup inside the global scope.
    SystemLinker::SearchResult ret =
        SystemLinker::Resolve(main_program_handle_, symbol_name);
    if (ret.IsValid()) {
      return ret.address;
    }

    // Then look inside the dependencies.
    for (const LibraryView* dep : *dependencies_) {
      // LOG("Looking into dependency %p (%s)", dep, dep->GetName());
      address = LookupIn(symbol_name, dep);
      if (address)
        return address;
    }

    // Nothing found here.
    return nullptr;
  }

 private:
  // Lookup for |symbol_name| inside of |lib|, and return the corresponding
  // address. For system libraries, this can also resolve missing "isnanf"
  // or "__isnanf" symbols from libm.so to local_isnanf. For crazy libraries,
  // this will look only within the library, not its dependencies.
  virtual void* LookupIn(const char* symbol_name, const LibraryView* lib) {
    if (lib->IsSystem()) {
      LibraryView::SearchResult sym = lib->LookupSymbol(symbol_name);
      // Android libm.so defines isnanf as weak. This means that its
      // address cannot be found by dlsym(), which returns NULL for weak
      // symbols prior to Android 5.0. However, libm.so contains the real
      // isnanf as __isnanf. If we encounter isnanf and fail to resolve
      // it in libm.so, retry with __isnanf.
      //
      // This occurs only in clang, which lacks __builtin_isnanf. The
      // gcc compiler implements isnanf as a builtin, so the symbol
      // isnanf never need be resolved in gcc builds.
      //
      // http://code.google.com/p/chromium/issues/detail?id=376828
      if (!sym.IsValid() && !strcmp(symbol_name, "isnanf") &&
          !strcmp(lib->GetName(), "libm.so")) {
        sym = lib->LookupSymbol("__isnanf");
        if (!sym.IsValid()) {
          // __isnanf only exists on Android 21+, so use a local fallback
          // if that doesn't exist either.
          sym.address = reinterpret_cast<void*>(&local_isnanf);
        }
      }
      return sym.address;
    }

    const SharedLibrary* crazy = lib->GetCrazy();
    if (crazy) {
      const ELF::Sym* entry = crazy->LookupSymbolEntry(symbol_name);
      if (entry)
        return reinterpret_cast<void*>(crazy->load_bias() + entry->st_value);
    }

    return nullptr;
  }

  void* main_program_handle_;
  SharedLibrary* lib_;
  const Vector<LibraryView*>* preloads_;
  const Vector<LibraryView*>* dependencies_;
};

}  // namespace

SharedLibrary::SharedLibrary() {
  full_path_[0] = '\0';
}

SharedLibrary::~SharedLibrary() = default;

bool SharedLibrary::Load(const LoadParams& params, Error* error) {
  // First, record the path.
  const char* full_path = params.library_path.c_str();
  if (params.library_fd >= 0) {
    snprintf(full_path_, sizeof(full_path_), "fd(%d):%s", params.library_fd,
             full_path);
  } else {
    size_t full_path_len = strlen(full_path);
    if (full_path_len >= sizeof(full_path_)) {
      error->Format("Path too long: %s", full_path);
      return false;
    }
    strlcpy(full_path_, full_path, sizeof(full_path_));
  }
  base_name_ = GetBaseNamePtr(full_path_);
  LOG("full path '%s'", full_path_);

  // Default value of |soname_| will be |base_name_| unless overidden
  // by a DT_SONAME entry. This helps deal with broken libraries that don't
  // have one. Note that starting with Android N, the system linker requires
  // every library to have a DT_SONAME, as these are used to uniquely identify
  // libraries for dependency resolution (barring namespace isolation).
  soname_ = base_name_;

  // Load the ELF binary in memory.
  LOG("Loading ELF segments for %s", base_name_);

  {
    ElfLoader::Result ret = ElfLoader::LoadAt(params, error);
    if (!ret.IsValid() ||
        !view_.InitUnmapped(ret.load_start, ret.phdr, ret.phdr_count, error)) {
      return false;
    }

    if (!symbols_.Init(&view_)) {
      *error = "Missing or malformed symbol table";
      return false;
    }

    reserved_map_ = std::move(ret.reserved_mapping);

    LOG("Reserved mapping %p size=0x%lx", reserved_map_.address(),
        static_cast<unsigned long>(reserved_map_.size()));
  }

  if (phdr_table_get_relro_info(view_.phdr(),
                                view_.phdr_count(),
                                view_.load_bias(),
                                &relro_start_,
                                &relro_size_) < 0) {
    relro_start_ = 0;
    relro_size_ = 0;
  }

#ifdef __arm__
  LOG("Extracting ARM.exidx table for %s", base_name_);
  (void)phdr_table_get_arm_exidx(
      phdr(), phdr_count(), load_bias(), &arm_exidx_, &arm_exidx_count_);
#endif

  LOG("Parsing dynamic table for %s", base_name_);
  ElfView::DynamicIterator dyn(&view_);
  RDebug* rdebug = Globals::GetRDebug();
  for (; dyn.HasNext(); dyn.GetNext()) {
    ELF::Addr dyn_value = dyn.GetValue();
    uintptr_t dyn_addr = dyn.GetAddress(load_bias());
    switch (dyn.GetTag()) {
      case DT_DEBUG:
        if (view_.dynamic_flags() & PF_W) {
          *dyn.GetValuePointer() =
              reinterpret_cast<uintptr_t>(rdebug->GetAddress());
        }
        break;
      case DT_INIT:
        LOG("  DT_INIT addr=%p", dyn_addr);
        init_func_ = reinterpret_cast<linker_function_t>(dyn_addr);
        break;
      case DT_FINI:
        LOG("  DT_FINI addr=%p", dyn_addr);
        fini_func_ = reinterpret_cast<linker_function_t>(dyn_addr);
        break;
      case DT_INIT_ARRAY:
        LOG("  DT_INIT_ARRAY addr=%p", dyn_addr);
        init_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_INIT_ARRAYSZ:
        init_array_count_ = dyn_value / sizeof(ELF::Addr);
        LOG("  DT_INIT_ARRAYSZ value=%p count=%p", dyn_value,
            init_array_count_);
        break;
      case DT_FINI_ARRAY:
        LOG("  DT_FINI_ARRAY addr=%p", dyn_addr);
        fini_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_FINI_ARRAYSZ:
        fini_array_count_ = dyn_value / sizeof(ELF::Addr);
        LOG("  DT_FINI_ARRAYSZ value=%p count=%p", dyn_value,
            fini_array_count_);
        break;
      case DT_PREINIT_ARRAY:
        LOG("  DT_PREINIT_ARRAY addr=%p", dyn_addr);
        preinit_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_PREINIT_ARRAYSZ:
        preinit_array_count_ = dyn_value / sizeof(ELF::Addr);
        LOG("  DT_PREINIT_ARRAYSZ value=%p count=%p", dyn_value,
            preinit_array_count_);
        break;
      case DT_SYMBOLIC:
        LOG("  DT_SYMBOLIC");
        has_DT_SYMBOLIC_ = true;
        break;
      case DT_FLAGS:
        if (dyn_value & DF_SYMBOLIC)
          has_DT_SYMBOLIC_ = true;
        break;
#if defined(__mips__)
      case DT_MIPS_RLD_MAP:
        *dyn.GetValuePointer() =
            reinterpret_cast<ELF::Addr>(rdebug->GetAddress());
        break;
#endif
      case DT_SONAME:
        soname_ = symbols_.string_table() + dyn_value;
        LOG("  DT_SONAME %s", soname_);
        break;

      default:
        ;
    }
  }

  LOG("Load complete for %s", base_name_);
  return true;
}

bool SharedLibrary::Relocate(LibraryList* lib_list,
                             const Vector<LibraryView*>* preloads,
                             const Vector<LibraryView*>* dependencies,
                             Error* error) {
  // Apply relocations.
  LOG("Applying relocations to %s", base_name_);

  ElfRelocations relocations;

  if (!relocations.Init(&view_, error))
    return false;

  SharedLibraryResolver resolver(this, lib_list, preloads, dependencies);
  if (!relocations.ApplyAll(&symbols_, &resolver, error))
    return false;

  LOG("Relocations applied for %s", base_name_);
  return true;
}

const ELF::Sym* SharedLibrary::LookupSymbolEntry(
    const char* symbol_name) const {
  return symbols_.LookupByName(symbol_name);
}

void* SharedLibrary::FindAddressForSymbol(const char* symbol_name) const {
  return symbols_.LookupAddressByName(symbol_name, view_.load_bias());
}

bool SharedLibrary::CreateSharedRelro(size_t load_address,
                                      size_t* relro_start,
                                      size_t* relro_size,
                                      int* relro_fd,
                                      Error* error) {
  SharedRelro relro;

  if (!relro.Allocate(relro_size_, base_name_, error))
    return false;

  if (load_address != 0 && load_address != this->load_address()) {
    // Need to relocate the content of the ashmem region first to accomodate
    // for the new load address.
    if (!relro.CopyFromRelocated(
             &view_, load_address, relro_start_, relro_size_, error))
      return false;
  } else {
    // Simply copy, no relocations.
    if (!relro.CopyFrom(relro_start_, relro_size_, error))
      return false;
  }

  // Enforce read-only mode for the region's content.
  if (!relro.ForceReadOnly(error))
    return false;

  // All good.
  *relro_start = relro.start();
  *relro_size = relro.size();
  *relro_fd = relro.DetachFd();
  return true;
}

bool SharedLibrary::UseSharedRelro(size_t relro_start,
                                   size_t relro_size,
                                   int relro_fd,
                                   Error* error) {
  LOG("relro_start=%p relro_size=%p relro_fd=%d", (void*)relro_start,
      (void*)relro_size, relro_fd);

  if (relro_fd < 0 || relro_size == 0) {
    // Nothing to do here.
    return true;
  }

  // Sanity check: A shared RELRO is not already used.
  if (relro_used_) {
    *error = "Library already using shared RELRO section";
    return false;
  }

  // Sanity check: RELRO addresses must match.
  if (relro_start_ != relro_start || relro_size_ != relro_size) {
    error->Format("RELRO mismatch addr=%p size=%p (wanted addr=%p size=%p)",
                  relro_start_,
                  relro_size_,
                  relro_start,
                  relro_size);
    return false;
  }

  // Everything's good, swap pages in this process's address space.
  SharedRelro relro;
  if (!relro.InitFrom(relro_start, relro_size, relro_fd, error))
    return false;

  relro_used_ = true;
  return true;
}

void SharedLibrary::CallConstructors() {
  CallFunction(init_func_, "DT_INIT");
  for (size_t n = 0; n < init_array_count_; ++n)
    CallFunction(init_array_[n], "DT_INIT_ARRAY");
}

void SharedLibrary::CallDestructors() {
  for (size_t n = fini_array_count_; n > 0; --n) {
    CallFunction(fini_array_[n - 1], "DT_FINI_ARRAY");
  }
  CallFunction(fini_func_, "DT_FINI");
}

bool SharedLibrary::CallJniOnLoad(void* java_vm,
                                  int minimum_jni_version,
                                  Error* error) {
  if (!java_vm)
    return true;

  // Lookup for JNI_OnLoad, exit if it doesn't exist.
  auto jni_onload = reinterpret_cast<JNI_OnLoadFunctionPtr>(
      FindAddressForSymbol("JNI_OnLoad"));
  if (!jni_onload)
    return true;

  int jni_version = (*jni_onload)(java_vm, NULL);
  if (jni_version < minimum_jni_version) {
    error->Format("JNI_OnLoad() in %s returned %d, expected at least %d",
                  full_path_,
                  jni_version,
                  minimum_jni_version);
    return false;
  }

  // Save the JavaVM handle for unload time.
  java_vm_ = java_vm;
  return true;
}

void SharedLibrary::CallJniOnUnload() {
  if (!java_vm_)
    return;

  JNI_OnUnloadFunctionPtr jni_on_unload =
      reinterpret_cast<JNI_OnUnloadFunctionPtr>(
          this->FindAddressForSymbol("JNI_OnUnload"));

  if (jni_on_unload)
    (*jni_on_unload)(java_vm_, NULL);
}

bool SharedLibrary::DependencyIterator::GetNext() {
  dep_name_ = NULL;
  for (; iter_.HasNext(); iter_.GetNext()) {
    if (iter_.GetTag() == DT_NEEDED) {
      dep_name_ = symbols_->GetStringById(iter_.GetValue());
      iter_.GetNext();
      return true;
    }
  }
  return false;
}

}  // namespace crazy
