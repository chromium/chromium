// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_library_view.h"

#include "crazy_linker_debug.h"
#include "crazy_linker_globals.h"
#include "crazy_linker_shared_library.h"
#include "crazy_linker_system_linker.h"

namespace crazy {

LibraryView::LibraryView(SharedLibrary* crazy_lib)
    : type_(TYPE_CRAZY), crazy_(crazy_lib), name_(crazy_lib->soname()) {}

LibraryView::~LibraryView() {
  LOG("Destroying %s", name_.c_str());
  if (type_ == TYPE_SYSTEM) {
    SystemLinker::Close(system_);
    system_ = NULL;
  } else if (type_ == TYPE_CRAZY) {
    delete crazy_;
    crazy_ = NULL;
  }
  type_ = TYPE_NONE;
}

LibraryView::SearchResult LibraryView::LookupSymbol(
    const char* symbol_name) const {
  if (type_ == TYPE_SYSTEM) {
    SystemLinker::SearchResult result =
        SystemLinker::Resolve(system_, symbol_name);
    if (result.IsValid()) {
      return {result.address, this};
    }
  } else if (type_ == TYPE_CRAZY) {
    LibraryList* lib_list = Globals::Get()->libraries();
    void* address = lib_list->FindSymbolFrom(symbol_name, this);
    if (address) {
      return {address, this};
    }
  }
  return {};
}

bool LibraryView::GetInfo(size_t* load_address,
                          size_t* load_size,
                          size_t* relro_start,
                          size_t* relro_size,
                          Error* error) const {
  if (type_ != TYPE_CRAZY) {
    *error = "No RELRO sharing with system libraries";
    return false;
  }
  crazy_->GetInfo(load_address, load_size, relro_start, relro_size);
  return true;
}

}  // namespace crazy
