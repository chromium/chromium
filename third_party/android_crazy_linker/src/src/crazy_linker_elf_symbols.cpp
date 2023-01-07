// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_symbols.h"

#include "crazy_linker_debug.h"
#include "crazy_linker_elf_view.h"

namespace crazy {

ElfSymbols::ElfSymbols(const ELF::Sym* symbol_table,
                       const char* string_table,
                       uintptr_t dt_elf_hash,
                       uintptr_t dt_gnu_hash)
    : symbol_table_(symbol_table), string_table_(string_table) {
  if (dt_elf_hash)
    elf_hash_.Init(dt_elf_hash);
  if (dt_gnu_hash)
    gnu_hash_.Init(dt_gnu_hash);
}

bool ElfSymbols::IsValid() const {
  return (symbol_table_ && string_table_ &&
          (gnu_hash_.IsValid() || elf_hash_.IsValid()));
}

bool ElfSymbols::Init(const ElfView* view) {
  LOG("Parsing dynamic table");
  ElfView::DynamicIterator dyn(view);
  for (; dyn.HasNext(); dyn.GetNext()) {
    uintptr_t dyn_addr = dyn.GetAddress(view->load_bias());
    switch (dyn.GetTag()) {
      case DT_HASH:
        LOG("  DT_HASH addr=%p", dyn_addr);
        elf_hash_.Init(dyn_addr);
        break;
      case DT_GNU_HASH:
        LOG("  DT_GNU_HASH addr=%p", dyn_addr);
        gnu_hash_.Init(dyn_addr);
        break;
      case DT_STRTAB:
        LOG("  DT_STRTAB addr=%p", dyn_addr);
        string_table_ = reinterpret_cast<const char*>(dyn_addr);
        break;
      case DT_SYMTAB:
        LOG("  DT_SYMTAB addr=%p", dyn_addr);
        symbol_table_ = reinterpret_cast<const ELF::Sym*>(dyn_addr);
        break;
      default:
        ;
    }
  }
  return IsValid();
}

const ELF::Sym* ElfSymbols::LookupByAddress(void* address,
                                            size_t load_bias) const {
  ELF::Addr elf_addr =
      reinterpret_cast<ELF::Addr>(address) - static_cast<ELF::Addr>(load_bias);

  for (const ELF::Sym& sym : GetDynSymbols()) {
    if (sym.st_shndx != SHN_UNDEF && elf_addr >= sym.st_value &&
        elf_addr < sym.st_value + sym.st_size) {
      return &sym;
    }
  }
  return nullptr;
}

bool ElfSymbols::LookupNearestByAddress(void* address,
                                        size_t load_bias,
                                        const char** sym_name,
                                        void** sym_addr,
                                        size_t* sym_size) const {
  ELF::Addr elf_addr =
      reinterpret_cast<ELF::Addr>(address) - static_cast<ELF::Addr>(load_bias);

  const ELF::Sym* nearest_sym = nullptr;
  size_t nearest_diff = ~size_t(0);

  for (const ELF::Sym& sym : GetDynSymbols()) {
    if (sym.st_shndx == SHN_UNDEF)
      continue;

    if (elf_addr >= sym.st_value && elf_addr < sym.st_value + sym.st_size) {
      // This is a perfect match.
      nearest_sym = &sym;
      break;
    }

    // Otherwise, compute distance.
    size_t diff;
    if (elf_addr < sym.st_value)
      diff = sym.st_value - elf_addr;
    else
      diff = elf_addr - sym.st_value - sym.st_size;

    if (diff < nearest_diff) {
      nearest_sym = &sym;
      nearest_diff = diff;
    }
  }

  if (!nearest_sym)
    return false;

  *sym_name = string_table_ + nearest_sym->st_name;
  *sym_addr = reinterpret_cast<void*>(nearest_sym->st_value + load_bias);
  *sym_size = nearest_sym->st_size;
  return true;
}

const ELF::Sym* ElfSymbols::LookupByName(const char* symbol_name) const {
  const ELF::Sym* sym =
      gnu_hash_.IsValid()
          ? gnu_hash_.LookupByName(symbol_name, symbol_table_, string_table_)
          : elf_hash_.LookupByName(symbol_name, symbol_table_, string_table_);

  // Ignore undefined symbols or those that are not global or weak definitions.
  if (!sym || sym->st_shndx == SHN_UNDEF)
    return nullptr;

  uint8_t info = ELF_ST_BIND(sym->st_info);
  if (info != STB_GLOBAL && info != STB_WEAK)
    return nullptr;

  return sym;
}

ElfSymbols::DynSymbols ElfSymbols::GetDynSymbols() const {
  if (gnu_hash_.IsValid()) {
    return {symbol_table_, gnu_hash_.dyn_symbols_offset(),
            gnu_hash_.dyn_symbols_count()};
  } else {
    return {symbol_table_, elf_hash_.dyn_symbols_offset(),
            elf_hash_.dyn_symbols_count()};
  }
}

}  // namespace crazy
