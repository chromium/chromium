// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_SYMBOLS_H
#define CRAZY_LINKER_ELF_SYMBOLS_H

#include "crazy_linker_elf_hash_table.h"
#include "crazy_linker_gnu_hash_table.h"
#include "elf_traits.h"

#include <string.h>

namespace crazy {

class ElfView;

// An ElfSymbols instance holds information about symbols in a mapped ELF
// binary.
class ElfSymbols {
 public:
  ElfSymbols() = default;

  // Constructor used for unit-testing.
  ElfSymbols(const ELF::Sym* symbol_table,
             const char* string_table,
             uintptr_t dt_elf_hash,
             uintptr_t dt_gnu_hash);

  // Returns true iff instance is valid.
  bool IsValid() const;

  // Initializes instance from |view|. Returns true on success, or false if
  // the ELF image is malformed.
  bool Init(const ElfView* view);

  // Returns the symbol table entry associated with |symbol_name|, or nullptr.
  const ELF::Sym* LookupByName(const char* symbol_name) const;

  // Returns the symbol table entry associated with |symbol_id|.
  const ELF::Sym* LookupById(size_t symbol_id) const {
    return &symbol_table_[symbol_id];
  }

  // Returns the symbol table entry corresponding to a given |address|.
  // |load_bias| must be the ELF image's load bias. Return nullptr if not found.
  const ELF::Sym* LookupByAddress(void* address, size_t load_bias) const;

  // Returns true iff symbol with id |symbol_id| is weak.
  bool IsWeakById(size_t symbol_id) const {
    return ELF_ST_BIND(symbol_table_[symbol_id].st_info) == STB_WEAK;
  }

  // Returns the name of the symbol associated with |symbol_id|.
  const char* LookupNameById(size_t symbol_id) const {
    const ELF::Sym* sym = LookupById(symbol_id);
    if (!sym)
      return nullptr;
    return string_table_ + sym->st_name;
  }

  // Returns the address of the symbol identified by |symbol_name|. |load_bias|
  // must be the ELF image's load bias. Return nullptr if not found.
  void* LookupAddressByName(const char* symbol_name, size_t load_bias) const {
    const ELF::Sym* sym = LookupByName(symbol_name);
    if (!sym)
      return nullptr;
    return reinterpret_cast<void*>(load_bias + sym->st_value);
  }

  // Lookups symbol information that is nearest to |address|, where |load_bias|
  // is the ELF image load bias. On success, return true and set |*sym_name|,
  // |*sym_addre| and |*sym_size|. On failure, return false.
  bool LookupNearestByAddress(void* address,
                              size_t load_bias,
                              const char** sym_name,
                              void** sym_addr,
                              size_t* sym_size) const;

  // Returns string identified by |str_id|.
  const char* GetStringById(size_t str_id) const {
    return string_table_ + str_id;
  }

  const char* string_table() const { return string_table_; }

 private:
  // Simple range view for the dynamic symbols within |symbol_table_|.
  // Provides begin() and end() to allow for-range loops.
  class DynSymbols {
   public:
    DynSymbols(const ELF::Sym* symbols, size_t start, size_t count)
        : begin_(symbols + start), end_(symbols + start + count) {}
    const ELF::Sym* begin() const { return begin_; }
    const ELF::Sym* end() const { return end_; }

   private:
    const ELF::Sym* begin_;
    const ELF::Sym* end_;
  };
  DynSymbols GetDynSymbols() const;

  const ELF::Sym* symbol_table_ = nullptr;
  const char* string_table_ = nullptr;
  ElfHashTable elf_hash_ = {};
  GnuHashTable gnu_hash_ = {};
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_SYMBOLS_H
