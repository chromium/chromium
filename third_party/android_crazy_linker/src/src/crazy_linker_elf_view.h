// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_VIEW_H
#define CRAZY_LINKER_ELF_VIEW_H

#include <string.h>

#include "crazy_linker_error.h"
#include "elf_traits.h"

namespace crazy {

class Error;

// An ElfView holds information describing a given ELF binary file for
// the crazy linker. This can be used to describe either system or crazy
// libraries.
class ElfView {
 public:
  ElfView() = default;

  // Initialize this ElfView from its load address and a copy of its program
  // header table.
  // |load_address| is the desired library load address.
  // |phdr| is a pointer to the library's program header. Note that this can
  // point to any memory location that contains a valid copy of the header.
  // I.e. the library does not have to be mapped in the process.
  // |phdr_count| number of entries in program header table.
  // On failure, return false and set |error| message.
  // On success, return true, and sets all fields of the ElfView to the
  // appropriate values. Note that functions phdr() or dynamic() will always
  // return an address relative to |load_address|, even if the binary was
  // not loaded yet in the process.
  bool InitUnmapped(ELF::Addr load_address,
                    const ELF::Phdr* phdr,
                    size_t phdr_count,
                    Error* error);

  const ELF::Phdr* phdr() const { return phdr_; }
  size_t phdr_count() const { return phdr_count_; }
  const ELF::Dyn* dynamic() const { return dynamic_; }
  size_t dynamic_count() const { return dynamic_count_; }
  size_t dynamic_flags() const { return dynamic_flags_; }
  size_t load_address() const { return load_address_; }
  size_t load_size() const { return load_size_; }
  size_t load_bias() const { return load_bias_; }

  // Helper class to iterate over the dynamic table.
  // Usage example:
  //     DynamicIterator iter;
  //     for ( ; iter.HasNext(); iter.SkipNext()) {
  //        if (iter.GetTag() == DT_SOME_TAG) {
  //           ... use iter.GetValue()
  //           ... or iter.GetAddress(load_address)
  //        }
  //     }
  class DynamicIterator {
   public:
    DynamicIterator(const ElfView* view) {
      dyn_ = view->dynamic();
      dyn_limit_ = dyn_ + view->dynamic_count();
    }

    ~DynamicIterator() {}

    bool HasNext() const { return dyn_ < dyn_limit_; }
    void GetNext() { dyn_ += 1; }

    ELF::Addr GetTag() const { return dyn_->d_tag; }

    ELF::Addr GetValue() const { return dyn_->d_un.d_val; }

    ELF::Addr* GetValuePointer() const {
      return const_cast<ELF::Addr*>(&dyn_->d_un.d_ptr);
    }

    uintptr_t GetOffset() const { return dyn_->d_un.d_ptr; }

    uintptr_t GetAddress(size_t load_bias) const {
      return load_bias + dyn_->d_un.d_ptr;
    }

   private:
    const ELF::Dyn* dyn_;
    const ELF::Dyn* dyn_limit_;
  };

  // Ensure the RELRO section is read-only after relocations. Assume the
  // ELF binary is mapped.On failure, return false and set |error| message.
  bool ProtectRelroSection(Error* error);

#if defined(__arm__) || defined(__aarch64__)
  // Register packed relocations to apply.
  // |packed_relocs| is a pointer to packed relocations data.
  void RegisterPackedRelocations(uint8_t* packed_relocations) {
    packed_relocations_ = packed_relocations;
  }

  uint8_t* packed_relocations() const { return packed_relocations_; }
#endif

 protected:
  const ELF::Phdr* phdr_ = nullptr;
  size_t phdr_count_ = 0;
  const ELF::Dyn* dynamic_ = nullptr;
  size_t dynamic_count_ = 0;
  ELF::Word dynamic_flags_ = 0;
  ELF::Addr load_address_ = 0;
  size_t load_size_ = 0;
  size_t load_bias_ = 0;

#if defined(__arm__) || defined(__aarch64__)
  uint8_t* packed_relocations_;
#endif
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_VIEW_H
