// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reloc_elf.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "components/zucchini/algorithm.h"

namespace zucchini {

/******** RelocReaderElf ********/

RelocReaderElf::RelocReaderElf(
    ConstBufferView image,
    Bitness bitness,
    const std::vector<SectionDimensionsElf>& reloc_section_dims,
    uint32_t rel_type,
    offset_t lo,
    offset_t hi,
    const AddressTranslator& translator)
    : image_(image),
      bitness_(bitness),
      rel_type_(rel_type),
      reloc_section_dimensions_(reloc_section_dims),
      hi_(hi),
      target_rva_to_offset_(translator) {
  DCHECK(bitness_ == kBit32 || bitness_ == kBit64);

  // Find the relocation section at or right before |lo|.
  cur_section_dimensions_ = std::upper_bound(
      reloc_section_dimensions_->begin(), reloc_section_dimensions_->end(), lo);
  if (cur_section_dimensions_ != reloc_section_dimensions_->begin())
    --cur_section_dimensions_;

  // |lo| and |hi_| do not cut across a reloc reference (e.g.,
  // Elf_Rel::r_offset), but may cut across a reloc struct (e.g. Elf_Rel)!
  // GetNext() emits all reloc references in |[lo, hi_)|, but needs to examine
  // the entire reloc struct for context. Knowing that |r_offset| is the first
  // entry in a reloc struct, |cursor_| and |hi_| are adjusted by the following:
  // - If |lo| is in a reloc section, then |cursor_| is chosen, as |lo| aligned
  //   up to the next reloc struct, to exclude reloc struct that |lo| may cut
  //   across.
  // - If |hi_| is in a reloc section, then align it up, to include reloc struct
  //   that |hi_| may cut across.
  cursor_ =
      base::checked_cast<offset_t>(cur_section_dimensions_->region.offset);
  if (cursor_ < lo)
    cursor_ +=
        AlignCeil<offset_t>(lo - cursor_, cur_section_dimensions_->entry_size);

  auto end_section = std::upper_bound(reloc_section_dimensions_->begin(),
                                      reloc_section_dimensions_->end(), hi_);
  if (end_section != reloc_section_dimensions_->begin()) {
    --end_section;
    if (hi_ - end_section->region.offset < end_section->region.size) {
      offset_t end_region_offset =
          base::checked_cast<offset_t>(end_section->region.offset);
      hi_ = end_region_offset + AlignCeil<offset_t>(hi_ - end_region_offset,
                                                    end_section->entry_size);
    }
  }
}

RelocReaderElf::~RelocReaderElf() = default;

rva_t RelocReaderElf::GetRelocationTarget(elf::Elf32_Rel rel) const {
  // The least significant byte of |rel.r_info| is the type. The other 3 bytes
  // store the symbol, which we ignore.
  uint32_t type = static_cast<uint32_t>(rel.r_info & 0xFF);
  if (type == rel_type_)
    return rel.r_offset;
  return kInvalidRva;
}

rva_t RelocReaderElf::GetRelocationTarget(elf::Elf64_Rel rel) const {
  // The least significant 4 bytes of |rel.r_info| is the type. The other 4
  // bytes store the symbol, which we ignore.
  uint32_t type = static_cast<uint32_t>(rel.r_info & 0xFFFFFFFF);
  if (type == rel_type_) {
    // Assume |rel.r_offset| fits within 32-bit integer.
    if ((rel.r_offset & 0xFFFFFFFF) == rel.r_offset)
      return static_cast<rva_t>(rel.r_offset);
    // Otherwise output warning.
    LOG(WARNING) << "Warning: Skipping r_offset whose value exceeds 32-bits.";
  }
  return kInvalidRva;
}

std::optional<Reference> RelocReaderElf::GetNext() {
  offset_t cur_entry_size = cur_section_dimensions_->entry_size;
  offset_t cur_section_dimensions_end =
      base::checked_cast<offset_t>(cur_section_dimensions_->region.hi());

  for (; cursor_ + cur_entry_size <= hi_; cursor_ += cur_entry_size) {
    while (cursor_ >= cur_section_dimensions_end) {
      ++cur_section_dimensions_;
      if (cur_section_dimensions_ == reloc_section_dimensions_->end())
        return std::nullopt;
      cur_entry_size = cur_section_dimensions_->entry_size;
      cursor_ =
          base::checked_cast<offset_t>(cur_section_dimensions_->region.offset);
      if (cursor_ + cur_entry_size > hi_)
        return std::nullopt;
      cur_section_dimensions_end =
          base::checked_cast<offset_t>(cur_section_dimensions_->region.hi());
    }
    rva_t target_rva = kInvalidRva;
    // TODO(huangs): Fix RELA sections: Need to process |r_addend|.
    switch (bitness_) {
      case kBit32:
        target_rva = GetRelocationTarget(image_.read<elf::Elf32_Rel>(cursor_));
        break;
      case kBit64:
        target_rva = GetRelocationTarget(image_.read<elf::Elf64_Rel>(cursor_));
        break;
    }
    if (target_rva == kInvalidRva)
      continue;
    // TODO(huangs): Make the check more strict: The reference body should not
    // straddle section boundary.
    offset_t target = target_rva_to_offset_.Convert(target_rva);
    if (target == kInvalidOffset)
      continue;
    // |target| will be used to obtain abs32 references, so we must ensure that
    // it lies inside |image_|.
    if (!image_.covers({target, WidthOf(bitness_)}))
      continue;
    offset_t location = cursor_;
    cursor_ += cur_entry_size;
    return Reference{location, target};
  }
  return std::nullopt;
}

/******** RelocWriterElf ********/

RelocWriterElf::RelocWriterElf(MutableBufferView image,
                               Bitness bitness,
                               const AddressTranslator& translator)
    : image_(image), bitness_(bitness), target_offset_to_rva_(translator) {
  DCHECK(bitness_ == kBit32 || bitness_ == kBit64);
}

RelocWriterElf::~RelocWriterElf() = default;

void RelocWriterElf::PutNext(Reference ref) {
  switch (bitness_) {
    case kBit32:
      image_.write<decltype(elf::Elf32_Rel::r_offset)>(
          ref.location + offsetof(elf::Elf32_Rel, r_offset),
          target_offset_to_rva_.Convert(ref.target));
      break;
    case kBit64:
      image_.write<decltype(elf::Elf64_Rel::r_offset)>(
          ref.location + offsetof(elf::Elf64_Rel, r_offset),
          target_offset_to_rva_.Convert(ref.target));
      break;
  }
  // Leave |reloc.r_info| alone.
}

}  // namespace zucchini
