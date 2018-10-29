// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_RELR_RELOCATIONS_H_
#define CRAZY_LINKER_RELR_RELOCATIONS_H_

#include "elf_traits.h"

namespace crazy {

// Convenience struct to model a set of RELR relocations and apply them.
// For more information about their format, see:
//    https://groups.google.com/forum/#!topic/generic-abi/bX460iggiKg
//
// In a nutshell, this looks like the following:
//
//  - The relr table is just an array of ELF:Addr values (i.e. 32 or 64 bit
//    words, depending on CPU bitness).
//
//  - Each relr relocation corresponds to simply applying the load bias
//    to a given memory location. I.e. applying one relocation at VIRTUAL
//    ADDRESS |vaddr| looks like:
//
//      *(reinterpret_cast<ELF::Addr*>(vaddr + load_bias)) += load_bias
//
//  - Even entries in the table corresponds to target virtual addresses,
//    where a RELR relocation should happen. Note that odd addresses are
//    not supported at all.
//
//  - Odd entries corresponds to bitmaps of 31 or 63 addresses following
//    the previous one (either from an even entry, or a previous odd one).
//    Each bit, after the lsb, that is set, means that the corresponding
//    address should be RELR-relocated.
//
class RelrRelocations {
 public:
  // Default constructor.
  RelrRelocations() = default;

  // Set the RELR address, |dt_relr| must be the DT_RELR entry from the
  // dynamic table.
  void SetAddress(uintptr_t dt_relr) {
    relocations_ =
        const_cast<const ELF::Relr*>(reinterpret_cast<ELF::Relr*>(dt_relr));
  }

  // Set the RELR size. |dt_relrsz| must be the DT_RELRSZ entry from the
  // dynamic table.
  void SetSize(ELF::Addr dt_relrsz) { relocations_size_ = dt_relrsz; }

  // Apply all relocations at once, where |load_bias| is the load load bias
  // used to load the ELF file. This operation cannot fail, and doesn't do
  // anything if there are no Relr relocations.
  void Apply(size_t load_bias);

 private:
  const ELF::Relr* relocations_ = nullptr;
  ELF::Addr relocations_size_ = 0;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_RELR_RELOCATIONS_H_
