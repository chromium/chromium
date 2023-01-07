// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_view.h"

#include <errno.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_error.h"
#include "linker_phdr.h"

namespace crazy {

bool ElfView::InitUnmapped(ELF::Addr load_address,
                           const ELF::Phdr* phdr,
                           size_t phdr_count,
                           Error* error) {
  // Compute load size and bias.
  ELF::Addr min_vaddr = 0;
  load_size_ = phdr_table_get_load_size(phdr, phdr_count, &min_vaddr, NULL);
  if (load_size_ == 0) {
    *error = "Invalid program header table";
    return false;
  }
  load_address_ = (load_address ? load_address : min_vaddr);
  load_bias_ = load_address - min_vaddr;

  // Extract the dynamic table information.
  phdr_table_get_dynamic_section(phdr,
                                 phdr_count,
                                 load_bias_,
                                 &dynamic_,
                                 &dynamic_count_,
                                 &dynamic_flags_);
  if (!dynamic_) {
    *error = "No PT_DYNAMIC section!";
    return false;
  }

  // Compute the program header table address relative to load_address.
  // This is different from |phdr|..|phdr + phdr_count| which can actually
  // be at a different location.
  const ELF::Phdr* phdr0 = NULL;

  // First, if there is a PT_PHDR, use it directly.
  for (size_t n = 0; n < phdr_count; ++n) {
    const ELF::Phdr* entry = &phdr[n];
    if (entry->p_type == PT_PHDR) {
      phdr0 = entry;
      break;
    }
  }

  // Otherwise, check the first loadable segment. If its file offset
  // is 0, it starts with the ELF header, and we can trivially find the
  // loaded program header from it.
  if (!phdr0) {
    for (size_t n = 0; n < phdr_count; ++n) {
      const ELF::Phdr* entry = &phdr[n];
      if (entry->p_type == PT_LOAD) {
        if (entry->p_offset == 0) {
          ELF::Addr elf_addr = load_bias_ + entry->p_vaddr;
          const ELF::Ehdr* ehdr = reinterpret_cast<const ELF::Ehdr*>(elf_addr);
          ELF::Addr offset = ehdr->e_phoff;
          phdr0 = reinterpret_cast<const ELF::Phdr*>(elf_addr + offset);
        }
        break;
      }
    }
  }

  // Check that the program header table is indeed in a loadable segment,
  // this helps catching malformed ELF binaries.
  if (phdr0) {
    ELF::Addr phdr0_addr = reinterpret_cast<ELF::Addr>(phdr0);
    ELF::Addr phdr0_limit = phdr0_addr + sizeof(ELF::Phdr) * phdr_count;
    bool found = false;
    for (size_t n = 0; n < phdr_count; ++n) {
      size_t seg_start = load_bias_ + phdr[n].p_vaddr;
      size_t seg_end = seg_start + phdr[n].p_filesz;

      if (seg_start <= phdr0_addr && phdr0_limit <= seg_end) {
        found = true;
        break;
      }
    }

    if (!found)
      phdr0 = NULL;
  }

  if (!phdr0) {
    *error = "Malformed ELF binary";
    return false;
  }

  phdr_ = phdr0;
  phdr_count_ = phdr_count;

  LOG("New ELF view [load_address:%p, load_size:%p, load_bias:%p, phdr:%p, "
      "phdr_count:%d, dynamic:%p, dynamic_count:%d, dynamic_flags:%d",
      load_address_, load_size_, load_bias_, phdr_, phdr_count_, dynamic_,
      dynamic_count_, dynamic_flags_);
  return true;
}

bool ElfView::ProtectRelroSection(Error* error) {
  LOG("Enabling GNU RELRO protection");

  if (phdr_table_protect_gnu_relro(phdr_, phdr_count_, load_bias_) < 0) {
    error->Format("Can't enable GNU RELRO protection: %s", strerror(errno));
    return false;
  }
  return true;
}

}  // namespace crazy
