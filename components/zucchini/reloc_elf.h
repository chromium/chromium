// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_RELOC_ELF_H_
#define COMPONENTS_ZUCCHINI_RELOC_ELF_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/type_elf.h"

namespace zucchini {

// Section dimensions for ELF files, to store relevant dimensions data from
// Elf32_Shdr and Elf64_Shdr, while reducing code duplication from templates.
struct SectionDimensionsElf {
  SectionDimensionsElf() = default;

  template <class Elf_Shdr>
  explicit SectionDimensionsElf(const Elf_Shdr& section)
      : region(BufferRegion{base::checked_cast<size_t>(section.sh_offset),
                            base::checked_cast<size_t>(section.sh_size)}),
        entry_size(base::checked_cast<offset_t>(section.sh_entsize)) {}

  friend bool operator<(const SectionDimensionsElf& a,
                        const SectionDimensionsElf& b) {
    return a.region.offset < b.region.offset;
  }

  friend bool operator<(offset_t offset, const SectionDimensionsElf& section) {
    return offset < section.region.offset;
  }

  BufferRegion region;
  offset_t entry_size;  // Varies across REL / RELA sections.
};

// A Generator to visit all reloc structs located in [|lo|, |hi|) (excluding
// truncated strct at |lo| but inlcuding truncated struct at |hi|), and emit
// valid References with |rel_type|. This implements a nested loop unrolled into
// a generator: the outer loop has |cur_section_dimensions_| visiting
// |reloc_section_dims| (sorted by |region.offset|), and the inner loop has
// |cursor_| visiting successive reloc structs within |cur_section_dimensions_|.
class RelocReaderElf : public ReferenceReader {
 public:
  RelocReaderElf(
      ConstBufferView image,
      Bitness bitness,
      const std::vector<SectionDimensionsElf>& reloc_section_dimensions,
      uint32_t rel_type,
      offset_t lo,
      offset_t hi,
      const AddressTranslator& translator);
  ~RelocReaderElf() override;

  // If |rel| contains |r_offset| for |rel_type_|, return the RVA. Otherwise
  // return |kInvalidRva|. These also handle Elf*_Rela, by using the fact that
  // Elf*_Rel is a prefix of Elf*_Rela.
  rva_t GetRelocationTarget(elf::Elf32_Rel rel) const;
  rva_t GetRelocationTarget(elf::Elf64_Rel rel) const;

  // ReferenceReader:
  std::optional<Reference> GetNext() override;

 private:
  const ConstBufferView image_;
  const Bitness bitness_;
  const uint32_t rel_type_;
  const raw_ref<const std::vector<SectionDimensionsElf>>
      reloc_section_dimensions_;
  std::vector<SectionDimensionsElf>::const_iterator cur_section_dimensions_;
  offset_t hi_;
  offset_t cursor_;
  AddressTranslator::RvaToOffsetCache target_rva_to_offset_;
};

class RelocWriterElf : public ReferenceWriter {
 public:
  RelocWriterElf(MutableBufferView image,
                 Bitness bitness,
                 const AddressTranslator& translator);
  ~RelocWriterElf() override;

  // ReferenceWriter:
  void PutNext(Reference ref) override;

 private:
  MutableBufferView image_;
  const Bitness bitness_;
  AddressTranslator::OffsetToRvaCache target_offset_to_rva_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_RELOC_ELF_H_
