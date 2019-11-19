// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_library_list.h"

#include <assert.h>
#include <crazy_linker.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_globals.h"
#include "crazy_linker_library_view.h"
#include "crazy_linker_pointer_set.h"
#include "crazy_linker_rdebug.h"
#include "crazy_linker_shared_library.h"
#include "crazy_linker_system.h"
#include "crazy_linker_system_linker.h"
#include "crazy_linker_util.h"

#ifdef __ANDROID__
#include "crazy_linker_system_android.h"
#endif

namespace crazy {

namespace {

// A helper struct used when looking up symbols in libraries.
struct SymbolLookupState {
  void* found_addr = nullptr;
  void* weak_addr = nullptr;
  int weak_count = 0;

  // Check a symbol entry.
  bool CheckSymbol(const char* symbol, const SharedLibrary* lib) {
    const ELF::Sym* entry = lib->LookupSymbolEntry(symbol);
    if (!entry)
      return false;

    void* address = reinterpret_cast<void*>(lib->load_bias() + entry->st_value);

    // If this is a strong symbol, record it and return true.
    if (ELF_ST_BIND(entry->st_info) == STB_GLOBAL) {
      found_addr = address;
      return true;
    }
    // If this is a weak symbol, record the first one and
    // increment the weak_count.
    if (++weak_count == 1)
      weak_addr = address;

    return false;
  }
};

// Checks that |params| is compatible with a system library load.
// On success return true. On failure, set |*error| and return false.
// |lib_name| is either the library name, or nullptr, in which case
// |params.library_path| will be used for the error message.
bool CheckSystemLibraryLoadParams(const char* lib_name,
                                  const LoadParams& params,
                                  Error* error) {
  if (!lib_name)
    lib_name = params.library_path.c_str();

  if (params.library_fd >= 0) {
    error->Format("Cannot load system library from fd %d: %s",
                  params.library_fd, lib_name);
    return false;
  }
  if (params.library_offset != 0) {
    error->Format("Cannot load system library from offset 0x%08lx: %s",
                  static_cast<unsigned long>(params.library_offset), lib_name);
    return false;
  }
  if (params.wanted_address != 0) {
    error->Format("Cannot load system library at address 0x%08lx: %s",
                  static_cast<unsigned long>(params.wanted_address), lib_name);
    return false;
  }
  if (params.reserved_size != 0) {
    error->Format("Cannot load system library in reserved memory map: %s",
                  lib_name);
  }
  return true;
}

}  // namespace

LibraryList::LibraryList() {
#ifdef __ANDROID__
  const int sdk_build_version = GetAndroidDeviceApiLevel();

  // If SDK version is Lollipop or earlier, we need to load anything
  // listed in LD_PRELOAD explicitly, because dlsym() on the main executable
  // fails to lookup in preloads on those releases. Also, when doing our
  // symbol resolution we need to explicity search preloads *before* we
  // search the main executable, to ensure that preloads override symbols
  // correctly. Searching preloads before main is opposite to the way the
  // system linker's ordering of these searches, but it is required here to
  // work round the platform's dlsym() issue.
  //
  // If SDK version is Lollipop-mr1 or later then dlsym() will search
  // preloads when invoked on the main executable, meaning that we do not
  // want (or need) to load them here. The platform itself will take care
  // of them for us, and so by not loading preloads here our preloads list
  // remains empty, so that searching it for name lookups is a no-op.
  //
  // For more, see:
  //   https://code.google.com/p/android/issues/detail?id=74255
  if (sdk_build_version <= ANDROID_SDK_VERSION_CODE_LOLLIPOP)
    LoadPreloads();
#endif  // __ANDROID__
}

LibraryList::~LibraryList() {
  // Invalidate crazy library list.
  head_ = nullptr;

  // Destroy all known libraries in reverse order.
  while (!known_libraries_.IsEmpty()) {
    LibraryView* view = known_libraries_.PopLast();
    delete view;
  }
}

void LibraryList::LoadPreloads() {
  const char* ld_preload = GetEnv("LD_PRELOAD");
  if (!ld_preload)
    return;

  LOG("Preloads list is: %s", ld_preload);
  const char* current = ld_preload;
  const char* end = ld_preload + strlen(ld_preload);

  // Iterate over library names listed in the environment. The separator
  // here may be either space or colon.
  while (current < end) {
    const char* item = current;
    const size_t item_length = strcspn(current, " :");
    if (item_length == 0) {
      current += 1;
      continue;
    }
    current = item + item_length + 1;

    String lib_name(item, item_length);
    LOG("Attempting to preload %s", lib_name.c_str());

    if (FindKnownLibrary(lib_name.c_str())) {
      LOG("already loaded %s: ignoring", lib_name.c_str());
      continue;
    }

    Error error;
    LibraryView* preload = LoadLibraryWithSystemLinker(
        lib_name.c_str(), RTLD_NOW | RTLD_GLOBAL, &error);
    if (!preload) {
      LOG("'%s' cannot be preloaded: ignored\n", lib_name.c_str());
      continue;
    }

    preloaded_libraries_.PushBack(preload);
  }

  if (CRAZY_DEBUG) {
    LOG("Preloads loaded");
    for (const LibraryView* preload : preloaded_libraries_)
      LOG("  ... %p %s\n", preload, preload->GetName());
    LOG("    preloads @%p\n", &preloaded_libraries_);
  }
}

LibraryView* LibraryList::FindLibraryByName(const char* lib_name) {
  // Sanity check.
  if (!lib_name)
    return nullptr;

  for (LibraryView* view : known_libraries_) {
    if (!strcmp(lib_name, view->GetName()))
      return view;
  }
  return nullptr;
}

void* LibraryList::FindSymbolFrom(const char* symbol_name,
                                  const LibraryView* from) {
  SymbolLookupState lookup_state;

  if (!from)
    return nullptr;

  // Use a work-queue and a set to ensure to perform a breadth-first
  // search.
  Vector<const LibraryView*> work_queue;
  PointerSet visited_set;

  work_queue.PushBack(from);

  while (!work_queue.IsEmpty()) {
    const LibraryView* lib = work_queue.PopFirst();
    const SharedLibrary* crazy_lib = lib->GetCrazy();
    if (crazy_lib) {
      if (lookup_state.CheckSymbol(symbol_name, crazy_lib))
        return lookup_state.found_addr;
    } else if (lib->IsSystem()) {
      LibraryView::SearchResult sym = lib->LookupSymbol(symbol_name);
      if (sym.IsValid())
        return sym.address;
    }

    // If this is a crazy library, add non-visited dependencies
    // to the work queue.
    if (crazy_lib) {
      SharedLibrary::DependencyIterator iter(crazy_lib);
      while (iter.GetNext()) {
        LibraryView* dependency = FindKnownLibrary(iter.GetName());
        if (dependency && !visited_set.Has(dependency)) {
          work_queue.PushBack(dependency);
          visited_set.Add(dependency);
        }
      }
    }
  }

  if (lookup_state.weak_count >= 1) {
    // There was at least a single weak symbol definition, so use
    // the first one found in breadth-first search order.
    return lookup_state.weak_addr;
  }

  // There was no symbol definition.
  return nullptr;
}

LibraryView* LibraryList::FindLibraryForAddress(void* address) {
  // Linearly scan all libraries, looking for one that contains
  // a given address. NOTE: This doesn't check that this falls
  // inside one of the mapped library segments.
  for (LibraryView* view : known_libraries_) {
    // TODO(digit): Search addresses inside system libraries.
    if (view->IsCrazy()) {
      SharedLibrary* lib = view->GetCrazy();
      if (lib->ContainsAddress(address))
        return view;
    }
  }
  return nullptr;
}

#ifdef __arm__
_Unwind_Ptr LibraryList::FindArmExIdx(void* pc, int* count) {
  for (SharedLibrary* lib = head_; lib; lib = lib->list_next_) {
    if (lib->ContainsAddress(pc)) {
      *count = static_cast<int>(lib->arm_exidx_count_);
      return reinterpret_cast<_Unwind_Ptr>(lib->arm_exidx_);
    }
  }
  *count = 0;
  return static_cast<_Unwind_Ptr>(NULL);
}
#else  // !__arm__
int LibraryList::IteratePhdr(PhdrIterationCallback callback, void* data) {
  int result = 0;
  for (SharedLibrary* lib = head_; lib; lib = lib->list_next_) {
    dl_phdr_info info;
    info.dlpi_addr = lib->link_map_.l_addr;
    info.dlpi_name = lib->link_map_.l_name;
    info.dlpi_phdr = lib->phdr();
    info.dlpi_phnum = lib->phdr_count();
    result = callback(&info, sizeof(info), data);
    if (result)
      break;
  }
  return result;
}
#endif  // !__arm__

void LibraryList::UnloadLibrary(LibraryView* wrap) {
  // Sanity check.
  LOG("for %s (ref_count=%d)", wrap->GetName(), wrap->ref_count());

  if (!wrap->IsSystem() && !wrap->IsCrazy())
    return;

  if (!wrap->SafeDecrementRef())
    return;

  // If this is a crazy library, perform manual cleanup first.
  SharedLibrary* lib = wrap->GetCrazy();
  if (lib) {
    // Remove from internal list of crazy libraries.
    if (lib->list_next_)
      lib->list_next_->list_prev_ = lib->list_prev_;
    if (lib->list_prev_)
      lib->list_prev_->list_next_ = lib->list_next_;
    if (lib == head_)
      head_ = lib->list_next_;

    // Call JNI_OnUnload, if necessary, then the destructors.
    LOG("Running JNI_OnUnload() for %s", wrap->GetName());
    lib->CallJniOnUnload();
    LOG("Running destructors for %s", wrap->GetName());
    lib->CallDestructors();

    // Unload the dependencies recursively.
    SharedLibrary::DependencyIterator iter(lib);
    while (iter.GetNext()) {
      LibraryView* dependency = FindKnownLibrary(iter.GetName());
      if (dependency)
        UnloadLibrary(dependency);
    }

    // Tell GDB of this removal.
    Globals::GetRDebug()->DelEntry(&lib->link_map_);
  }

  known_libraries_.Remove(wrap);

  // Delete the wrapper, which will delete the crazy library, or
  // dlclose() the system one.
  delete wrap;
}

LibraryView* LibraryList::LoadLibraryWithSystemLinker(const char* lib_name,
                                                      int dlopen_mode,
                                                      Error* error) {
  // First check whether a library with the same base name was
  // already loaded.
  ASSERT(!FindKnownLibrary(lib_name),
         "System library already loaded: ", lib_name);

  void* system_lib = SystemLinker::Open(lib_name, dlopen_mode);
  if (!system_lib) {
    error->Format("Can't load system library %s: %s", lib_name,
                  SystemLinker::Error());
    return nullptr;
  }

  // Can't really find the DT_SONAME of this library, assume if is its basename.
  LibraryView* view = new LibraryView(system_lib, GetBaseNamePtr(lib_name));
  known_libraries_.PushBack(view);

  LOG("System library %s loaded at %p", lib_name, view);
  return view;
}

Expected<LibraryView*> LibraryList::FindAndCheckLoadedLibrary(
    const char* lib_name,
    const LoadParams& params,
    Error* error) {
  // First check whether a library with the same base name was already loaded.
  LibraryView* view = FindKnownLibrary(lib_name);
  if (!view)
    return nullptr;

  if (view->IsSystem()) {
    if (!CheckSystemLibraryLoadParams(lib_name, params, error))
      return error;
  } else {
    const SharedLibrary* crazy_lib = view->GetCrazy();
    ASSERT(crazy_lib != nullptr, "Not a crazy library");
    if (params.wanted_address) {
      // Check that this is a crazy library and that is was loaded at
      // the correct address.
      uintptr_t actual_address = crazy_lib->load_address();
      if (actual_address != params.wanted_address) {
        error->Format(
            "Library already loaded at address 0x%08lx, can't load it at "
            "0x%08lx: %s",
            static_cast<unsigned long>(actual_address),
            static_cast<unsigned long>(params.wanted_address), lib_name);
        return error;
      }
    }
  }

  view->AddRef();
  return view;
}

// static
bool LibraryList::LocateLibraryFile(const char* lib_name,
                                    const SearchPathList& search_path_list,
                                    LoadParams* params,
                                    Error* error) {
  LOG("Looking through the search path list");
  SearchPathList::Result probe = search_path_list.FindFile(lib_name);
  if (!probe.IsValid()) {
    error->Format("Can't find library file %s", lib_name);
    return false;
  }
  LOG("Found library: path %s @ 0x%x", probe.path.c_str(), probe.offset);
  params->library_path = std::move(probe.path);
  params->library_offset = probe.offset;
  return true;
}

LibraryView* LibraryList::LoadLibrary(const char* lib_name,
                                      const LoadParams& params,
                                      Error* error) {
  Expected<LibraryView*> found =
      FindAndCheckLoadedLibrary(lib_name, params, error);
  if (!found.has_value())
    return nullptr;
  if (found.value())
    return found.value();
  return LoadLibraryInternal(params, error);
}

LibraryView* LibraryList::LoadLibraryInternal(const LoadParams& params,
                                              Error* error) {
  // Load the library with the system linker if necessary.
  const char* lib_path = params.library_path.c_str();
  if (IsSystemLibraryPath(lib_path)) {
    if (!CheckSystemLibraryLoadParams(lib_path, params, error))
      return nullptr;
    return LoadLibraryWithSystemLinker(lib_path, RTLD_NOW, error);
  }

  // Load the library with the crazy linker.
  ScopedPtr<SharedLibrary> lib(new SharedLibrary());
  if (!lib->Load(params, error))
    return nullptr;

  // Load all dependendent libraries.
  const char* base_name = GetBaseNamePtr(lib_path);
  LOG("Loading dependencies of %s", base_name);
  SharedLibrary::DependencyIterator iter(lib.Get());
  Vector<LibraryView*> dependencies;
  while (iter.GetNext()) {
    Error dep_error;
    // TODO(digit): Better library dependency loading that isn't limited
    // to system libraries. This would allow the linker to load anything
    // without the caller having to load all dependencies before hand in
    // reverse topological order.
    const char* dependency_name = iter.GetName();
    LibraryView* dependency = FindKnownLibrary(dependency_name);
    if (!dependency) {
      dependency =
          LoadLibraryWithSystemLinker(dependency_name, RTLD_NOW, &dep_error);
      if (!dependency) {
        error->Format("When loading %s: %s", base_name, dep_error.c_str());
        // TODO(digit): Unload all dependencies that were loaded so far.
        return nullptr;
      }
    }
    dependencies.PushBack(dependency);
  }
  if (CRAZY_DEBUG) {
    LOG("Dependencies loaded for %s", base_name);
    for (const LibraryView* dep : dependencies)
      LOG("  ... %p %s", dep, dep->GetName());
    LOG("    dependencies @%p", &dependencies);
  }

  // Relocate the library.
  LOG("Relocating %s", base_name);
  if (!lib->Relocate(this, &preloaded_libraries_, &dependencies, error))
    return nullptr;

  // Notify GDB of load.
  lib->link_map_.l_addr = lib->load_bias();
  lib->link_map_.l_name = const_cast<char*>(lib->base_name_);
  lib->link_map_.l_ld = reinterpret_cast<uintptr_t>(lib->view_.dynamic());
  Globals::GetRDebug()->AddEntry(&lib->link_map_);

  // The library was properly loaded, add it to the list of crazy
  // libraries. IMPORTANT: Do this _before_ calling the constructors
  // because these could call dlopen().
  lib->list_next_ = head_;
  lib->list_prev_ = nullptr;
  if (head_)
    head_->list_prev_ = lib.Get();
  head_ = lib.Get();

  // Then create a new LibraryView for it.
  LibraryView* view = new LibraryView(lib.Release());
  known_libraries_.PushBack(view);

  LOG("Running constructors for %s", base_name);
  // Now run the constructors.
  view->GetCrazy()->CallConstructors();

  // Then try to call JNI_OnLoad() if necessary.
  LOG("Running JNI_OnLoad() for %s", base_name);
  Globals* globals = Globals::Get();
  if (!view->GetCrazy()->CallJniOnLoad(globals->java_vm(),
                                       globals->minimum_jni_version(), error)) {
    LOG("Error on JNI_OnLoad(): %s", error->c_str());
    UnloadLibrary(view);
    return nullptr;
  }

  LOG("Done loading %s", base_name);
  return view;
}

void LibraryList::AddLibrary(LibraryView* wrap) {
  known_libraries_.PushBack(wrap);
}

LibraryView* LibraryList::FindKnownLibrary(const char* name) {
  const char* base_name = GetBaseNamePtr(name);
  for (LibraryView* view : known_libraries_) {
    if (!strcmp(base_name, view->GetName()))
      return view;
  }
  return nullptr;
}

}  // namespace crazy
