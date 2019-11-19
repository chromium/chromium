// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_win32.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "components/zucchini/abs32_utils.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/rel32_finder.h"
#include "components/zucchini/rel32_utils.h"
#include "components/zucchini/reloc_win32.h"

namespace zucchini {

namespace {

// Decides whether |image| points to a Win32 PE file. If this is a possibility,
// assigns |source| to enable further parsing, and returns true. Otherwise
// leaves |source| at an undefined state and returns false.
template <class Traits>
bool ReadWin32Header(ConstBufferView image, BufferSource* source) {
  *source = BufferSource(image);

  // Check "MZ" magic of DOS header.
  if (!source->CheckNextBytes({'M', 'Z'}))
    return false;

  const auto* dos_header = source->GetPointer<pe::ImageDOSHeader>();
  // For |e_lfanew|, reject on misalignment or overlap with DOS header.
  if (!dos_header || (dos_header->e_lfanew & 7) != 0 ||
      dos_header->e_lfanew < 0U + sizeof(pe::ImageDOSHeader)) {
    return false;
  }
  // Offset to PE header is in DOS header.
  *source = std::move(BufferSource(image).Skip(dos_header->e_lfanew));
  // Check 'PE\0\0' magic from PE header.
  if (!source->ConsumeBytes({'P', 'E', 0, 0}))
    return false;

  return true;
}

template <class Traits>
const pe::ImageDataDirectory* ReadDataDirectory(
    const typename Traits::ImageOptionalHeader* optional_header,
    size_t index) {
  if (index >= optional_header->number_of_rva_and_sizes)
    return nullptr;
  return &optional_header->data_directory[index];
}

// Decides whether |section| (assumed value) is a section that contains code.
template <class Traits>
bool IsWin32CodeSection(const pe::ImageSectionHeader& section) {
  return (section.characteristics & kCodeCharacteristics) ==
         kCodeCharacteristics;
}

}  // namespace

/******** Win32X86Traits ********/

// static
constexpr Bitness Win32X86Traits::kBitness;
constexpr ExecutableType Win32X86Traits::kExeType;
const char Win32X86Traits::kExeTypeString[] = "Windows PE x86";

/******** Win32X64Traits ********/

// static
constexpr Bitness Win32X64Traits::kBitness;
constexpr ExecutableType Win32X64Traits::kExeType;
const char Win32X64Traits::kExeTypeString[] = "Windows PE x64";

/******** DisassemblerWin32 ********/

// static.
template <class Traits>
bool DisassemblerWin32<Traits>::QuickDetect(ConstBufferView image) {
  BufferSource source;
  return ReadWin32Header<Traits>(image, &source);
}

// |num_equivalence_iterations_| = 2 for reloc -> abs32.
template <class Traits>
DisassemblerWin32<Traits>::DisassemblerWin32() : Disassembler(2) {}

template <class Traits>
DisassemblerWin32<Traits>::~DisassemblerWin32() = default;

template <class Traits>
ExecutableType DisassemblerWin32<Traits>::GetExeType() const {
  return Traits::kExeType;
}

template <class Traits>
std::string DisassemblerWin32<Traits>::GetExeTypeString() const {
  return Traits::kExeTypeString;
}

template <class Traits>
std::vector<ReferenceGroup> DisassemblerWin32<Traits>::MakeReferenceGroups()
    const {
  return {
      {ReferenceTypeTraits{2, TypeTag(kReloc), PoolTag(kReloc)},
       &DisassemblerWin32::MakeReadRelocs, &DisassemblerWin32::MakeWriteRelocs},
      {ReferenceTypeTraits{Traits::kVAWidth, TypeTag(kAbs32), PoolTag(kAbs32)},
       &DisassemblerWin32::MakeReadAbs32, &DisassemblerWin32::MakeWriteAbs32},
      {ReferenceTypeTraits{4, TypeTag(kRel32), PoolTag(kRel32)},
       &DisassemblerWin32::MakeReadRel32, &DisassemblerWin32::MakeWriteRel32},
  };
}

template <class Traits>
std::unique_ptr<ReferenceReader> DisassemblerWin32<Traits>::MakeReadRelocs(
    offset_t lo,
    offset_t hi) {
  if (!ParseAndStoreRelocBlocks())
    return std::make_unique<EmptyReferenceReader>();

  RelocRvaReaderWin32 reloc_rva_reader(image_, reloc_region_,
                                       reloc_block_offsets_, lo, hi);
  CHECK_GE(image_.size(), Traits::kVAWidth);
  offset_t offset_bound =
      base::checked_cast<offset_t>(image_.size() - Traits::kVAWidth + 1);
  return std::make_unique<RelocReaderWin32>(std::move(reloc_rva_reader),
                                            Traits::kRelocType, offset_bound,
                                            translator_);
}

template <class Traits>
std::unique_ptr<ReferenceReader> DisassemblerWin32<Traits>::MakeReadAbs32(
    offset_t lo,
    offset_t hi) {
  ParseAndStoreAbs32();
  Abs32RvaExtractorWin32 abs_rva_extractor(
      image_, {Traits::kBitness, image_base_}, abs32_locations_, lo, hi);
  return std::make_unique<Abs32ReaderWin32>(std::move(abs_rva_extractor),
                                            translator_);
}

template <class Traits>
std::unique_ptr<ReferenceReader> DisassemblerWin32<Traits>::MakeReadRel32(
    offset_t lo,
    offset_t hi) {
  ParseAndStoreRel32();
  return std::make_unique<Rel32ReaderX86>(image_, lo, hi, &rel32_locations_,
                                          translator_);
}

template <class Traits>
std::unique_ptr<ReferenceWriter> DisassemblerWin32<Traits>::MakeWriteRelocs(
    MutableBufferView image) {
  if (!ParseAndStoreRelocBlocks())
    return std::make_unique<EmptyReferenceWriter>();

  return std::make_unique<RelocWriterWin32>(Traits::kRelocType, image,
                                            reloc_region_, reloc_block_offsets_,
                                            translator_);
}

template <class Traits>
std::unique_ptr<ReferenceWriter> DisassemblerWin32<Traits>::MakeWriteAbs32(
    MutableBufferView image) {
  return std::make_unique<Abs32WriterWin32>(
      image, AbsoluteAddress(Traits::kBitness, image_base_), translator_);
}

template <class Traits>
std::unique_ptr<ReferenceWriter> DisassemblerWin32<Traits>::MakeWriteRel32(
    MutableBufferView image) {
  return std::make_unique<Rel32WriterX86>(image, translator_);
}

template <class Traits>
bool DisassemblerWin32<Traits>::Parse(ConstBufferView image) {
  image_ = image;
  return ParseHeader();
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseHeader() {
  BufferSource source;

  if (!ReadWin32Header<Traits>(image_, &source))
    return false;

  constexpr size_t kDataDirBase =
      offsetof(typename Traits::ImageOptionalHeader, data_directory);
  auto* coff_header = source.GetPointer<pe::ImageFileHeader>();
  if (!coff_header || coff_header->size_of_optional_header < kDataDirBase)
    return false;

  // |number_of_rva_and_sizes < kImageNumberOfDirectoryEntries| is possible. So
  // in theory, GetPointer() on ImageOptionalHeader can reach EOF for a tiny PE
  // file, causing false rejection. However, this should not occur for practical
  // cases; and rejection is okay for corner cases (e.g., from a fuzzer).
  auto* optional_header =
      source.GetPointer<typename Traits::ImageOptionalHeader>();
  if (!optional_header || optional_header->magic != Traits::kMagic)
    return false;

  // Check |optional_header->number_of_rva_and_sizes|.
  const size_t data_dir_size =
      coff_header->size_of_optional_header - kDataDirBase;
  const size_t num_data_dir = data_dir_size / sizeof(pe::ImageDataDirectory);
  if (num_data_dir != optional_header->number_of_rva_and_sizes ||
      num_data_dir * sizeof(pe::ImageDataDirectory) != data_dir_size ||
      num_data_dir > pe::kImageNumberOfDirectoryEntries) {
    return false;
  }

  base_relocation_table_ = ReadDataDirectory<Traits>(
      optional_header, pe::kIndexOfBaseRelocationTable);
  if (!base_relocation_table_)
    return false;

  image_base_ = optional_header->image_base;

  // |optional_header->size_of_image| is the size of the image when loaded into
  // memory, and not the actual size on disk.
  rva_t rva_bound = optional_header->size_of_image;
  if (rva_bound >= kRvaBound)
    return false;

  // An exclusive upper bound of all offsets used in the image. This gets
  // updated as sections get visited.
  offset_t offset_bound =
      base::checked_cast<offset_t>(source.begin() - image_.begin());

  // Extract |sections_|.
  size_t sections_count = coff_header->number_of_sections;
  auto* sections_array =
      source.GetArray<pe::ImageSectionHeader>(sections_count);
  if (!sections_array)
    return false;
  sections_.assign(sections_array, sections_array + sections_count);

  // Prepare |units| for offset-RVA translation.
  std::vector<AddressTranslator::Unit> units;
  units.reserve(sections_count);

  // Visit each section, validate, and add address translation data to |units|.
  bool has_text_section = false;
  decltype(pe::ImageSectionHeader::virtual_address) prev_virtual_address = 0;
  for (size_t i = 0; i < sections_count; ++i) {
    const pe::ImageSectionHeader& section = sections_[i];
    // Apply strict checks on section bounds.
    if (!image_.covers(
            {section.file_offset_of_raw_data, section.size_of_raw_data})) {
      return false;
    }
    if (!RangeIsBounded(section.virtual_address, section.virtual_size,
                        rva_bound)) {
      return false;
    }

    // PE sections should be sorted by RVAs. For robustness, we don't rely on
    // this, so even if unsorted we don't care. Output warning though.
    if (prev_virtual_address > section.virtual_address)
      LOG(WARNING) << "RVA anomaly found for Section " << i;
    prev_virtual_address = section.virtual_address;

    // Add |section| data for offset-RVA translation.
    units.push_back({section.file_offset_of_raw_data, section.size_of_raw_data,
                     section.virtual_address, section.virtual_size});

    offset_t end_offset =
        section.file_offset_of_raw_data + section.size_of_raw_data;
    offset_bound = std::max(end_offset, offset_bound);
    if (IsWin32CodeSection<Traits>(section))
      has_text_section = true;
  }

  if (offset_bound > image_.size())
    return false;
  if (!has_text_section)
    return false;

  // Initialize |translator_| for offset-RVA translations. Any inconsistency
  // (e.g., 2 offsets correspond to the same RVA) would invalidate the PE file.
  if (translator_.Initialize(std::move(units)) != AddressTranslator::kSuccess)
    return false;

  // Resize |image_| to include only contents claimed by sections. Note that
  // this may miss digital signatures at end of PE files, but for patching this
  // is of minor concern.
  image_.shrink(offset_bound);

  return true;
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseAndStoreRelocBlocks() {
  if (has_parsed_relocs_)
    return reloc_region_.lo() != kInvalidOffset;

  has_parsed_relocs_ = true;
  DCHECK(reloc_block_offsets_.empty());

  offset_t relocs_offset =
      translator_.RvaToOffset(base_relocation_table_->virtual_address);
  size_t relocs_size = base_relocation_table_->size;
  const BufferRegion temp_reloc_region = {relocs_offset, relocs_size};

  // Reject bogus relocs. It's possible to have no reloc, so this is non-fatal!
  if (relocs_offset == kInvalidOffset || !image_.covers(temp_reloc_region))
    return false;

  // Precompute offsets of all reloc blocks.
  if (!RelocRvaReaderWin32::FindRelocBlocks(image_, temp_reloc_region,
                                            &reloc_block_offsets_)) {
    return false;
  }
  // Reassign |reloc_region_| only on success.
  reloc_region_ = temp_reloc_region;
  return true;
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseAndStoreAbs32() {
  if (has_parsed_abs32_)
    return true;
  has_parsed_abs32_ = true;

  // Read reloc targets as preliminary abs32 locations.
  std::unique_ptr<ReferenceReader> relocs = MakeReadRelocs(0, offset_t(size()));
  for (auto ref = relocs->GetNext(); ref.has_value(); ref = relocs->GetNext())
    abs32_locations_.push_back(ref->target);

  std::sort(abs32_locations_.begin(), abs32_locations_.end());

  // Abs32 references must have targets translatable to offsets. Remove those
  // that are unable to do so.
  size_t num_untranslatable = RemoveUntranslatableAbs32(
      image_, {Traits::kBitness, image_base_}, translator_, &abs32_locations_);
  LOG_IF(WARNING, num_untranslatable) << "Removed " << num_untranslatable
                                      << " untranslatable abs32 references.";

  // Abs32 reference bodies must not overlap. If found, simply remove them.
  size_t num_overlapping =
      RemoveOverlappingAbs32Locations(Traits::kVAWidth, &abs32_locations_);
  LOG_IF(WARNING, num_overlapping)
      << "Removed " << num_overlapping
      << " abs32 references with overlapping bodies.";

  abs32_locations_.shrink_to_fit();
  return true;
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseAndStoreRel32() {
  if (has_parsed_rel32_)
    return true;
  has_parsed_rel32_ = true;

  ParseAndStoreAbs32();

  AddressTranslator::OffsetToRvaCache location_offset_to_rva(translator_);
  AddressTranslator::RvaToOffsetCache target_rva_checker(translator_);

  for (const pe::ImageSectionHeader& section : sections_) {
    if (!IsWin32CodeSection<Traits>(section))
      continue;

    rva_t start_rva = section.virtual_address;
    rva_t end_rva = start_rva + section.virtual_size;

    // |virtual_size < size_of_raw_data| is possible. In this case, disassembly
    // should not proceed beyond |virtual_size|, so rel32 location RVAs remain
    // translatable to file offsets.
    uint32_t size_to_use =
        std::min(section.virtual_size, section.size_of_raw_data);
    ConstBufferView region =
        image_[{section.file_offset_of_raw_data, size_to_use}];
    Abs32GapFinder gap_finder(image_, region, abs32_locations_,
                              Traits::kVAWidth);
    typename Traits::RelFinder finder;
    // Iterate over gaps between abs32 references, to avoid collision.
    for (auto gap = gap_finder.GetNext(); gap.has_value();
         gap = gap_finder.GetNext()) {
      finder.SetRegion(gap.value());
      // Iterate over heuristically detected rel32 references, validate, and add
      // to |rel32_locations_|.
      for (auto rel32 = finder.GetNext(); rel32.has_value();
           rel32 = finder.GetNext()) {
        offset_t rel32_offset = offset_t(rel32->location - image_.begin());
        rva_t rel32_rva = location_offset_to_rva.Convert(rel32_offset);
        DCHECK_NE(rel32_rva, kInvalidRva);
        rva_t target_rva = rel32_rva + 4 + image_.read<uint32_t>(rel32_offset);
        if (target_rva_checker.IsValid(target_rva) &&
            (rel32->can_point_outside_section ||
             (start_rva <= target_rva && target_rva < end_rva))) {
          finder.Accept();
          rel32_locations_.push_back(rel32_offset);
        }
      }
    }
  }
  rel32_locations_.shrink_to_fit();
  // |sections_| entries are usually sorted by offset, but there's no guarantee.
  // So sort explicitly, to be sure.
  std::sort(rel32_locations_.begin(), rel32_locations_.end());
  return true;
}

// Explicit instantiation for supported classes.
template class DisassemblerWin32<Win32X86Traits>;
template class DisassemblerWin32<Win32X64Traits>;

}  // namespace zucchini
