// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_RELOCATIONS_H
#define CRAZY_LINKER_ELF_RELOCATIONS_H

#include <string.h>
#include <unistd.h>

#include <link.h>

#include "crazy_linker_defines.h"
#include "crazy_linker_relr_relocations.h"
#include "elf_traits.h"

namespace crazy {

class ElfSymbols;
class ElfView;
class Error;

// An ElfRelocations instance holds information about relocations in a mapped
// ELF binary.
class ElfRelocations {
#if defined(USE_RELA)
  typedef ELF::Rela rel_t;
#else
  typedef ELF::Rel rel_t;
#endif
 public:
  ElfRelocations();
  ~ElfRelocations() {}

  bool Init(const ElfView* view, Error* error);

  // Abstract class used to resolve symbol names into addresses.
  // Callers of ::ApplyAll() should pass the address of a derived class
  // that properly implements the Lookup() method.
  class SymbolResolver {
   public:
    SymbolResolver() {}
    ~SymbolResolver() {}
    virtual void* Lookup(const char* symbol_name) = 0;
  };

  // Apply all relocations to the target mapped ELF binary. Must be called
  // after Init().
  // |symbols| maps to the symbol entries for the target library only.
  // |resolver| can resolve symbols out of the current library.
  // On error, return false and set |error| message.
  bool ApplyAll(const ElfSymbols* symbols,
                SymbolResolver* resolver,
                Error* error);

  // This function is used to adjust relocated addresses in a copy of an
  // existing section of an ELF binary. I.e. |src_addr|...|src_addr + size|
  // must be inside the mapped ELF binary, this function will first copy its
  // content into |dst_addr|...|dst_addr + size|, then adjust all relocated
  // addresses inside the destination section as if it was loaded/mapped
  // at |map_addr|...|map_addr + size|. Only relative relocations are processed,
  // symbolic ones are ignored.
  void CopyAndRelocate(size_t src_addr,
                       size_t dst_addr,
                       size_t map_addr,
                       size_t size);

 private:
  bool ResolveSymbol(unsigned rel_type,
                     unsigned rel_symbol,
                     const ElfSymbols* symbols,
                     SymbolResolver* resolver,
                     ELF::Addr reloc,
                     ELF::Addr* sym_addr,
                     Error* error);
  bool ApplyResolvedReloc(const rel_t* rela,
                          ELF::Addr sym_addr,
                          bool resolved,
                          Error* error);
  bool ApplyReloc(const rel_t* rela,
                  const ElfSymbols* symbols,
                  SymbolResolver* resolver,
                  Error* error);
  bool ApplyRelocs(const rel_t* relocs,
                   size_t relocs_count,
                   const ElfSymbols* symbols,
                   SymbolResolver* resolver,
                   Error* error);
  void AdjustRelocation(ELF::Word rel_type,
                        ELF::Addr src_reloc,
                        size_t dst_delta,
                        size_t map_delta);
  void RelocateRelocations(size_t src_addr,
                          size_t dst_addr,
                          size_t map_addr,
                          size_t size);
  void AdjustAndroidRelocation(const rel_t* relocation,
                               size_t src_addr,
                               size_t dst_addr,
                               size_t map_addr,
                               size_t size);

  // Android packed relocations unpacker. Calls the given handler for
  // each relocation in the unpacking stream.
  typedef bool (*RelocationHandler)(ElfRelocations* relocations,
                                    const rel_t* relocation,
                                    void* opaque);
  bool ForEachAndroidRelocation(RelocationHandler handler,
                                void* opaque);
  template <typename ElfRelIteratorT>
  bool ForEachAndroidRelocationHelper(ElfRelIteratorT&& rel_iterator,
                                      ElfRelocations::RelocationHandler handler,
                                      void* opaque);

  // Apply Android packed relocations.
  // On error, return false and set |error| message.
  // The static function is the ForEachAndroidRelocation() handler.
  bool ApplyAndroidRelocations(const ElfSymbols* symbols,
                               SymbolResolver* resolver,
                               Error* error);
  static bool ApplyAndroidRelocation(ElfRelocations* relocations,
                                     const rel_t* relocation,
                                     void* opaque);

  // Relocate Android packed relocations.
  // The static function is the ForEachAndroidRelocation() handler.
  void RelocateAndroidRelocations(size_t src_addr,
                                  size_t dst_addr,
                                  size_t map_addr,
                                  size_t size);
  static bool RelocateAndroidRelocation(ElfRelocations* relocations,
                                        const rel_t* relocation,
                                        void* opaque);

#if defined(__mips__)
  bool RelocateMipsGot(const ElfSymbols* symbols,
                       SymbolResolver* resolver,
                       Error* error);
#endif

  const ELF::Phdr* phdr_ = nullptr;
  size_t phdr_count_ = 0;
  size_t load_bias_ = 0;

  ELF::Addr plt_relocations_ = 0;
  size_t plt_relocations_size_ = 0;
  ELF::Addr* plt_got_ = nullptr;

  ELF::Addr relocations_ = 0;
  size_t relocations_size_ = 0;

  RelrRelocations relr_;

#if defined(__mips__)
  // MIPS-specific relocation fields.
  ELF::Word mips_symtab_count_ = 0;
  ELF::Word mips_local_got_count_ = 0;
  ELF::Word mips_gotsym_ = 0;
#endif

  uint8_t* android_relocations_ = nullptr;
  size_t android_relocations_size_ = 0;

  bool has_text_relocations_ = false;
  bool has_symbolic_ = false;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_RELOCATIONS_H
