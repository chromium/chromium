// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/disassembler_elf.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/zucchini/abs32_utils.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/arm_utils.h"
#include "components/zucchini/buffer_source.h"

namespace zucchini {

namespace {

constexpr uint64_t kElfImageBase = 0;
constexpr size_t kSizeBound = 0x7FFF0000;

// Threshold value for heuristics to detect THUMB2 code.
constexpr double kAArch32BitCondAlwaysDensityThreshold = 0.4;

// Bit fields for JudgeSection() return value.
enum SectionJudgement : int {
  // Bit: Section does not invalidate ELF, but may or may not be useful.
  SECTION_BIT_SAFE = 1 << 0,
  // Bit: Section useful for AddressTranslator, to map between offsets and RVAs.
  SECTION_BIT_USEFUL_FOR_ADDRESS_TRANSLATOR = 1 << 1,
  // Bit: Section useful for |offset_bound|, to estimate ELF size.
  SECTION_BIT_USEFUL_FOR_OFFSET_BOUND = 1 << 2,
  // Bit: Section potentially useful for pointer extraction.
  SECTION_BIT_MAYBE_USEFUL_FOR_POINTERS = 1 << 3,

  // The following are verdicts from combining bits, to improve semantics.
  // Default value: A section is malformed and invalidates ELF.
  SECTION_IS_MALFORMED = 0,
  // Section does not invalidate ELF, but is also not used for anything.
  SECTION_IS_USELESS = SECTION_BIT_SAFE,
};

// Decides how a section affects ELF parsing, and returns a bit field composed
// from SectionJudgement values.
template <class TRAITS>
int JudgeSection(size_t image_size, const typename TRAITS::Elf_Shdr* section) {
  // BufferRegion uses |size_t| this can be 32-bit in some cases. For Elf64
  // |sh_addr|, |sh_offset| and |sh_size| are 64-bit this can result in
  // overflows in the subsequent validation steps.
  if (!base::IsValueInRangeForNumericType<size_t>(section->sh_addr) ||
      !base::IsValueInRangeForNumericType<size_t>(section->sh_offset) ||
      !base::IsValueInRangeForNumericType<size_t>(section->sh_size)) {
    return SECTION_IS_MALFORMED;
  }

  // Examine RVA range: Reject if numerical overflow may happen.
  if (!BufferRegion{static_cast<size_t>(section->sh_addr),
                    static_cast<size_t>(section->sh_size)}
           .FitsIn(kSizeBound))
    return SECTION_IS_MALFORMED;

  // Examine offset range: If section takes up |image| data then be stricter.
  size_t offset_bound =
      (section->sh_type == elf::SHT_NOBITS) ? kSizeBound : image_size;
  if (!BufferRegion{static_cast<size_t>(section->sh_offset),
                    static_cast<size_t>(section->sh_size)}
           .FitsIn(offset_bound))
    return SECTION_IS_MALFORMED;

  // Empty sections don't contribute to offset-RVA mapping. For consistency, it
  // should also not affect |offset_bounds|.
  if (section->sh_size == 0)
    return SECTION_IS_USELESS;

  // Sections with |sh_addr == 0| are ignored because these tend to duplicates
  // (can cause problems for lookup) and uninteresting. For consistency, it
  // should also not affect |offset_bounds|.
  if (section->sh_addr == 0)
    return SECTION_IS_USELESS;

  if (section->sh_type == elf::SHT_NOBITS) {
    // Special case for .tbss sections: These should be ignored because they may
    // have offset-RVA map that don't match other sections.
    if (section->sh_flags & elf::SHF_TLS)
      return SECTION_IS_USELESS;

    // Section is useful for offset-RVA translation, but does not affect
    // |offset_bounds| since it can have large virtual size (e.g., .bss).
    return SECTION_BIT_SAFE | SECTION_BIT_USEFUL_FOR_ADDRESS_TRANSLATOR;
  }

  return SECTION_BIT_SAFE | SECTION_BIT_USEFUL_FOR_ADDRESS_TRANSLATOR |
         SECTION_BIT_USEFUL_FOR_OFFSET_BOUND |
         SECTION_BIT_MAYBE_USEFUL_FOR_POINTERS;
}

// Determines whether |section| is a reloc section.
template <class TRAITS>
bool IsRelocSection(const typename TRAITS::Elf_Shdr& section) {
  DCHECK_GT(section.sh_size, 0U);
  if (section.sh_type == elf::SHT_REL) {
    // Also validate |section.sh_entsize|, which gets used later.
    return section.sh_entsize == sizeof(typename TRAITS::Elf_Rel);
  }
  if (section.sh_type == elf::SHT_RELA)
    return section.sh_entsize == sizeof(typename TRAITS::Elf_Rela);
  return false;
}

// Determines whether |section| is a section with executable code.
template <class TRAITS>
bool IsExecSection(const typename TRAITS::Elf_Shdr& section) {
  DCHECK_GT(section.sh_size, 0U);
  return section.sh_type == elf::SHT_PROGBITS &&
         (section.sh_flags & elf::SHF_EXECINSTR) != 0;
}

}  // namespace

/******** Elf32Traits ********/

// static
constexpr Bitness Elf32Traits::kBitness;
constexpr elf::FileClass Elf32Traits::kIdentificationClass;

/******** Elf32IntelTraits ********/

// static
constexpr ExecutableType Elf32IntelTraits::kExeType;
const char Elf32IntelTraits::kExeTypeString[] = "ELF x86";
constexpr elf::MachineArchitecture Elf32IntelTraits::kMachineValue;
constexpr uint32_t Elf32IntelTraits::kRelType;

/******** ElfAArch32Traits ********/

// static
constexpr ExecutableType ElfAArch32Traits::kExeType;
const char ElfAArch32Traits::kExeTypeString[] = "ELF ARM";
constexpr elf::MachineArchitecture ElfAArch32Traits::kMachineValue;
constexpr uint32_t ElfAArch32Traits::kRelType;

/******** Elf64Traits ********/

// static
constexpr Bitness Elf64Traits::kBitness;
constexpr elf::FileClass Elf64Traits::kIdentificationClass;

/******** Elf64IntelTraits ********/

// static
constexpr ExecutableType Elf64IntelTraits::kExeType;
const char Elf64IntelTraits::kExeTypeString[] = "ELF x64";
constexpr elf::MachineArchitecture Elf64IntelTraits::kMachineValue;
constexpr uint32_t Elf64IntelTraits::kRelType;

/******** ElfAArch64Traits ********/

// static
constexpr ExecutableType ElfAArch64Traits::kExeType;
const char ElfAArch64Traits::kExeTypeString[] = "ELF ARM64";
constexpr elf::MachineArchitecture ElfAArch64Traits::kMachineValue;
constexpr uint32_t ElfAArch64Traits::kRelType;

/******** DisassemblerElf ********/

// static.
template <class TRAITS>
bool DisassemblerElf<TRAITS>::QuickDetect(ConstBufferView image) {
  BufferSource source(image);

  // Do not consume the bytes for the magic value, as they are part of the
  // header.
  if (!source.CheckNextBytes({0x7F, 'E', 'L', 'F'}))
    return false;

  auto* header = source.GetPointer<typename Traits::Elf_Ehdr>();
  if (!header)
    return false;

  if (header->e_ident[elf::EI_CLASS] != Traits::kIdentificationClass)
    return false;

  if (header->e_ident[elf::EI_DATA] != 1)  // Only ELFDATA2LSB is supported.
    return false;

  if (header->e_type != elf::ET_EXEC && header->e_type != elf::ET_DYN)
    return false;

  if (header->e_version != 1 || header->e_ident[elf::EI_VERSION] != 1)
    return false;

  if (header->e_machine != supported_architecture())
    return false;

  if (header->e_shentsize != sizeof(typename Traits::Elf_Shdr))
    return false;

  return true;
}

template <class TRAITS>
DisassemblerElf<TRAITS>::~DisassemblerElf() = default;

template <class TRAITS>
ExecutableType DisassemblerElf<TRAITS>::GetExeType() const {
  return Traits::kExeType;
}

template <class TRAITS>
std::string DisassemblerElf<TRAITS>::GetExeTypeString() const {
  return Traits::kExeTypeString;
}

// |num_equivalence_iterations_| = 2 for reloc -> abs32.
template <class TRAITS>
DisassemblerElf<TRAITS>::DisassemblerElf() : Disassembler(2) {}

template <class TRAITS>
bool DisassemblerElf<TRAITS>::Parse(ConstBufferView image) {
  image_ = image;
  if (!ParseHeader())
    return false;
  ParseSections();
  return true;
}

template <class TRAITS>
std::unique_ptr<ReferenceReader> DisassemblerElf<TRAITS>::MakeReadRelocs(
    offset_t lo,
    offset_t hi) {
  DCHECK_LE(lo, hi);
  DCHECK_LE(hi, image_.size());

  if (reloc_section_dims_.empty())
    return std::make_unique<EmptyReferenceReader>();

  return std::make_unique<RelocReaderElf>(
      image_, Traits::kBitness, reloc_section_dims_,
      supported_relocation_type(), lo, hi, translator_);
}

template <class TRAITS>
std::unique_ptr<ReferenceWriter> DisassemblerElf<TRAITS>::MakeWriteRelocs(
    MutableBufferView image) {
  return std::make_unique<RelocWriterElf>(image, Traits::kBitness, translator_);
}

template <class TRAITS>
bool DisassemblerElf<TRAITS>::ParseHeader() {
  BufferSource source(image_);
  // Ensure any offsets will fit within the |image_|'s bounds.
  if (!base::IsValueInRangeForNumericType<offset_t>(image_.size()))
    return false;

  // Ensures |header_| is valid later on.
  if (!QuickDetect(image_))
    return false;

  header_ = source.GetPointer<typename Traits::Elf_Ehdr>();

  sections_count_ = header_->e_shnum;
  source = BufferSource(image_, header_->e_shoff);
  sections_ = source.GetArray<typename Traits::Elf_Shdr>(sections_count_);
  if (!sections_)
    return false;
  offset_t section_table_end =
      base::checked_cast<offset_t>(source.begin() - image_.begin());

  segments_count_ = header_->e_phnum;
  source = BufferSource(image_, header_->e_phoff);
  segments_ = source.GetArray<typename Traits::Elf_Phdr>(segments_count_);
  if (!segments_)
    return false;
  offset_t segment_table_end =
      base::checked_cast<offset_t>(source.begin() - image_.begin());

  // Check string section -- even though we've stopped using them.
  elf::Elf32_Half string_section_id = header_->e_shstrndx;
  if (string_section_id >= sections_count_)
    return false;
  size_t section_names_size = sections_[string_section_id].sh_size;
  if (section_names_size > 0) {
    // If nonempty, then last byte of string section must be null.
    const char* section_names = nullptr;
    source = BufferSource(image_, sections_[string_section_id].sh_offset);
    section_names = source.GetArray<char>(section_names_size);
    if (!section_names || section_names[section_names_size - 1] != '\0')
      return false;
  }

  // Establish bound on encountered offsets.
  offset_t offset_bound = std::max(section_table_end, segment_table_end);

  // Visits |segments_| to get estimate on |offset_bound|.
  for (const typename Traits::Elf_Phdr* segment = segments_;
       segment != segments_ + segments_count_; ++segment) {
    // |image_.covers()| is a sufficient check except when size_t is 32 bit and
    // parsing ELF64. In such cases a value-in-range check is needed on the
    // segment. This fixes crbug/1035603.
    offset_t segment_end;
    base::CheckedNumeric<offset_t> checked_segment_end = segment->p_offset;
    checked_segment_end += segment->p_filesz;
    if (!checked_segment_end.AssignIfValid(&segment_end) ||
        !image_.covers({static_cast<size_t>(segment->p_offset),
                        static_cast<size_t>(segment->p_filesz)})) {
      return false;
    }
    offset_bound = std::max(offset_bound, segment_end);
  }

  // Visit and validate each section; add address translation data to |units|.
  std::vector<AddressTranslator::Unit> units;
  units.reserve(sections_count_);
  section_judgements_.reserve(sections_count_);

  for (int i = 0; i < sections_count_; ++i) {
    const typename Traits::Elf_Shdr* section = &sections_[i];
    int judgement = JudgeSection<Traits>(image_.size(), section);
    section_judgements_.push_back(judgement);
    if ((judgement & SECTION_BIT_SAFE) == 0)
      return false;

    uint32_t sh_size = base::checked_cast<uint32_t>(section->sh_size);
    offset_t sh_offset = base::checked_cast<offset_t>(section->sh_offset);
    rva_t sh_addr = base::checked_cast<rva_t>(section->sh_addr);
    if ((judgement & SECTION_BIT_USEFUL_FOR_ADDRESS_TRANSLATOR) != 0) {
      // Store mappings between RVA and offset.
      units.push_back({sh_offset, sh_size, sh_addr, sh_size});
    }
    if ((judgement & SECTION_BIT_USEFUL_FOR_OFFSET_BOUND) != 0) {
      offset_t section_end = base::checked_cast<offset_t>(sh_offset + sh_size);
      offset_bound = std::max(offset_bound, section_end);
    }
  }

  // Initialize |translator_| for offset-RVA translations. Any inconsistency
  // (e.g., 2 offsets correspond to the same RVA) would invalidate the ELF file.
  if (translator_.Initialize(std::move(units)) != AddressTranslator::kSuccess)
    return false;

  DCHECK_LE(offset_bound, image_.size());
  image_.shrink(offset_bound);
  return true;
}

template <class TRAITS>
void DisassemblerElf<TRAITS>::ExtractInterestingSectionHeaders() {
  DCHECK(reloc_section_dims_.empty());
  DCHECK(exec_headers_.empty());
  for (elf::Elf32_Half i = 0; i < sections_count_; ++i) {
    const typename Traits::Elf_Shdr* section = sections_ + i;
    if ((section_judgements_[i] & SECTION_BIT_MAYBE_USEFUL_FOR_POINTERS) != 0) {
      if (IsRelocSection<Traits>(*section))
        reloc_section_dims_.emplace_back(*section);
      else if (IsExecSection<Traits>(*section))
        exec_headers_.push_back(section);
    }
  }
  auto comp = [](const typename Traits::Elf_Shdr* a,
                 const typename Traits::Elf_Shdr* b) {
    return a->sh_offset < b->sh_offset;
  };
  std::sort(reloc_section_dims_.begin(), reloc_section_dims_.end());
  std::sort(exec_headers_.begin(), exec_headers_.end(), comp);
}

template <class TRAITS>
void DisassemblerElf<TRAITS>::GetAbs32FromRelocSections() {
  constexpr int kAbs32Width = Traits::kVAWidth;
  DCHECK(abs32_locations_.empty());

  // Read reloc targets to get preliminary abs32 locations.
  std::unique_ptr<ReferenceReader> relocs = MakeReadRelocs(0, offset_t(size()));
  for (auto ref = relocs->GetNext(); ref.has_value(); ref = relocs->GetNext())
    abs32_locations_.push_back(ref->target);

  std::sort(abs32_locations_.begin(), abs32_locations_.end());

  // Abs32 references must have targets translatable to offsets. Remove those
  // that are unable to do so.
  size_t num_untranslatable =
      RemoveUntranslatableAbs32(image_, {Traits::kBitness, kElfImageBase},
                                translator_, &abs32_locations_);
  LOG_IF(WARNING, num_untranslatable) << "Removed " << num_untranslatable
                                      << " untranslatable abs32 references.";

  // Abs32 reference bodies must not overlap. If found, simply remove them.
  size_t num_overlapping =
      RemoveOverlappingAbs32Locations(kAbs32Width, &abs32_locations_);
  LOG_IF(WARNING, num_overlapping)
      << "Removed " << num_overlapping
      << " abs32 references with overlapping bodies.";

  abs32_locations_.shrink_to_fit();
}

template <class TRAITS>
void DisassemblerElf<TRAITS>::GetRel32FromCodeSections() {
  for (const typename Traits::Elf_Shdr* section : exec_headers_)
    ParseExecSection(*section);
  PostProcessRel32();
}

template <class TRAITS>
void DisassemblerElf<TRAITS>::ParseSections() {
  ExtractInterestingSectionHeaders();
  GetAbs32FromRelocSections();
  GetRel32FromCodeSections();
}

/******** DisassemblerElfIntel ********/

template <class TRAITS>
DisassemblerElfIntel<TRAITS>::DisassemblerElfIntel() = default;

template <class TRAITS>
DisassemblerElfIntel<TRAITS>::~DisassemblerElfIntel() = default;

template <class TRAITS>
std::vector<ReferenceGroup> DisassemblerElfIntel<TRAITS>::MakeReferenceGroups()
    const {
  return {
      {ReferenceTypeTraits{sizeof(TRAITS::Elf_Rel::r_offset), TypeTag(kReloc),
                           PoolTag(kReloc)},
       &DisassemblerElfIntel<TRAITS>::MakeReadRelocs,
       &DisassemblerElfIntel<TRAITS>::MakeWriteRelocs},
      {ReferenceTypeTraits{Traits::kVAWidth, TypeTag(kAbs32), PoolTag(kAbs32)},
       &DisassemblerElfIntel<TRAITS>::MakeReadAbs32,
       &DisassemblerElfIntel<TRAITS>::MakeWriteAbs32},
      // N.B.: Rel32 |width| is 4 bytes, even for x64.
      {ReferenceTypeTraits{4, TypeTag(kRel32), PoolTag(kRel32)},
       &DisassemblerElfIntel<TRAITS>::MakeReadRel32,
       &DisassemblerElfIntel<TRAITS>::MakeWriteRel32}};
}

template <class TRAITS>
void DisassemblerElfIntel<TRAITS>::ParseExecSection(
    const typename TRAITS::Elf_Shdr& section) {
  constexpr int kAbs32Width = Traits::kVAWidth;

  // |this->| is needed to access protected members of templated base class. To
  // reduce noise, use local references for these.
  ConstBufferView& image_ = this->image_;
  const AddressTranslator& translator_ = this->translator_;
  auto& abs32_locations_ = this->abs32_locations_;

  // Range of values was ensured in ParseHeader().
  rva_t start_rva = base::checked_cast<rva_t>(section.sh_addr);
  rva_t end_rva = base::checked_cast<rva_t>(start_rva + section.sh_size);

  AddressTranslator::RvaToOffsetCache target_rva_checker(translator_);

  ConstBufferView region(image_.begin() + section.sh_offset, section.sh_size);
  Abs32GapFinder gap_finder(image_, region, abs32_locations_, kAbs32Width);
  typename TRAITS::Rel32FinderUse rel_finder(image_, translator_);
  // Iterate over gaps between abs32 references, to avoid collision.
  while (gap_finder.FindNext()) {
    rel_finder.SetRegion(gap_finder.GetGap());
    while (rel_finder.FindNext()) {
      auto rel32 = rel_finder.GetRel32();
      if (target_rva_checker.IsValid(rel32.target_rva) &&
          (rel32.can_point_outside_section ||
           (start_rva <= rel32.target_rva && rel32.target_rva < end_rva))) {
        rel_finder.Accept();
        rel32_locations_.push_back(rel32.location);
      }
    }
  }
}

template <class TRAITS>
void DisassemblerElfIntel<TRAITS>::PostProcessRel32() {
  rel32_locations_.shrink_to_fit();
  std::sort(rel32_locations_.begin(), rel32_locations_.end());
}

template <class TRAITS>
std::unique_ptr<ReferenceReader> DisassemblerElfIntel<TRAITS>::MakeReadAbs32(
    offset_t lo,
    offset_t hi) {
  // TODO(huangs): Don't use Abs32RvaExtractorWin32 here; use new class that
  // caters to different ELF architectures.
  Abs32RvaExtractorWin32 abs_rva_extractor(
      this->image_, AbsoluteAddress(TRAITS::kBitness, kElfImageBase),
      this->abs32_locations_, lo, hi);
  return std::make_unique<Abs32ReaderWin32>(std::move(abs_rva_extractor),
                                            this->translator_);
}

template <class TRAITS>
std::unique_ptr<ReferenceWriter> DisassemblerElfIntel<TRAITS>::MakeWriteAbs32(
    MutableBufferView image) {
  return std::make_unique<Abs32WriterWin32>(
      image, AbsoluteAddress(TRAITS::kBitness, kElfImageBase),
      this->translator_);
}

template <class TRAITS>
std::unique_ptr<ReferenceReader> DisassemblerElfIntel<TRAITS>::MakeReadRel32(
    offset_t lo,
    offset_t hi) {
  return std::make_unique<Rel32ReaderX86>(this->image_, lo, hi,
                                          &rel32_locations_, this->translator_);
}

template <class TRAITS>
std::unique_ptr<ReferenceWriter> DisassemblerElfIntel<TRAITS>::MakeWriteRel32(
    MutableBufferView image) {
  return std::make_unique<Rel32WriterX86>(image, this->translator_);
}

// Explicit instantiation for supported classes.
template class DisassemblerElfIntel<Elf32IntelTraits>;
template class DisassemblerElfIntel<Elf64IntelTraits>;
template bool DisassemblerElf<Elf32IntelTraits>::QuickDetect(
    ConstBufferView image);
template bool DisassemblerElf<Elf64IntelTraits>::QuickDetect(
    ConstBufferView image);

/******** DisassemblerElfArm ********/

template <class Traits>
DisassemblerElfArm<Traits>::DisassemblerElfArm() = default;

template <class Traits>
DisassemblerElfArm<Traits>::~DisassemblerElfArm() = default;

template <class Traits>
bool DisassemblerElfArm<Traits>::IsTargetOffsetInExecSection(
    offset_t offset) const {
  // Executable sections can appear in large numbers in .o files and in
  // pathological cases. Since this function may be called for each reference
  // candidate, linear search may be too slow (so use binary search).
  return IsTargetOffsetInElfSectionList(this->exec_headers_, offset);
}

template <class Traits>
void DisassemblerElfArm<Traits>::ParseExecSection(
    const typename Traits::Elf_Shdr& section) {
  ConstBufferView& image_ = this->image_;
  const AddressTranslator& translator_ = this->translator_;
  auto& abs32_locations_ = this->abs32_locations_;

  ConstBufferView region(image_.begin() + section.sh_offset, section.sh_size);
  Abs32GapFinder gap_finder(image_, region, abs32_locations_, Traits::kVAWidth);
  std::unique_ptr<typename Traits::Rel32FinderUse> rel_finder =
      MakeRel32Finder(section);
  AddressTranslator::RvaToOffsetCache rva_to_offset(translator_);
  while (gap_finder.FindNext()) {
    rel_finder->SetRegion(gap_finder.GetGap());
    while (rel_finder->FindNext()) {
      auto rel32 = rel_finder->GetRel32();
      offset_t target_offset = rva_to_offset.Convert(rel32.target_rva);
      if (target_offset != kInvalidOffset) {
        // For robustness, reject illegal offsets, which can arise from, e.g.,
        // misidentify ARM vs. THUMB2 mode, or even misidentifying data as code!
        if (IsTargetOffsetInExecSection(target_offset)) {
          rel_finder->Accept();
          rel32_locations_table_[rel32.type].push_back(rel32.location);
        }
      }
    }
  }
}

template <class Traits>
void DisassemblerElfArm<Traits>::PostProcessRel32() {
  for (int type = 0; type < AArch32Rel32Translator::NUM_ADDR_TYPE; ++type) {
    std::sort(rel32_locations_table_[type].begin(),
              rel32_locations_table_[type].end());
    rel32_locations_table_[type].shrink_to_fit();
  }
}

template <class Traits>
std::unique_ptr<ReferenceReader> DisassemblerElfArm<Traits>::MakeReadAbs32(
    offset_t lo,
    offset_t hi) {
  // TODO(huangs): Reconcile the use of Win32-specific classes in ARM code!
  Abs32RvaExtractorWin32 abs_rva_extractor(this->image_,
                                           AbsoluteAddress(Traits::kBitness, 0),
                                           this->abs32_locations_, lo, hi);
  return std::make_unique<Abs32ReaderWin32>(std::move(abs_rva_extractor),
                                            this->translator_);
}

template <class Traits>
std::unique_ptr<ReferenceWriter> DisassemblerElfArm<Traits>::MakeWriteAbs32(
    MutableBufferView image) {
  return std::make_unique<Abs32WriterWin32>(
      image, AbsoluteAddress(Traits::kBitness, 0), this->translator_);
}

template <class TRAITS>
template <class ADDR_TRAITS>
std::unique_ptr<ReferenceReader> DisassemblerElfArm<TRAITS>::MakeReadRel32(
    offset_t lower,
    offset_t upper) {
  return std::make_unique<Rel32ReaderArm<ADDR_TRAITS>>(
      this->translator_, this->image_,
      this->rel32_locations_table_[ADDR_TRAITS::addr_type], lower, upper);
}

template <class TRAITS>
template <class ADDR_TRAITS>
std::unique_ptr<ReferenceWriter> DisassemblerElfArm<TRAITS>::MakeWriteRel32(
    MutableBufferView image) {
  return std::make_unique<Rel32WriterArm<ADDR_TRAITS>>(this->translator_,
                                                       image);
}

template <class TRAITS>
template <class ADDR_TRAITS>
std::unique_ptr<ReferenceMixer> DisassemblerElfArm<TRAITS>::MakeMixRel32(
    ConstBufferView src_image,
    ConstBufferView dst_image) {
  return std::make_unique<Rel32MixerArm<ADDR_TRAITS>>(src_image, dst_image);
}

/******** DisassemblerElfAArch32 ********/

DisassemblerElfAArch32::DisassemblerElfAArch32() = default;
DisassemblerElfAArch32::~DisassemblerElfAArch32() = default;

std::vector<ReferenceGroup> DisassemblerElfAArch32::MakeReferenceGroups()
    const {
  return {
      {ReferenceTypeTraits{sizeof(Traits::Elf_Rel::r_offset),
                           TypeTag(AArch32ReferenceType::kReloc),
                           PoolTag(ArmReferencePool::kPoolReloc)},
       &DisassemblerElfAArch32::MakeReadRelocs,
       &DisassemblerElfAArch32::MakeWriteRelocs},
      {ReferenceTypeTraits{Traits::kVAWidth,
                           TypeTag(AArch32ReferenceType::kAbs32),
                           PoolTag(ArmReferencePool::kPoolAbs32)},
       &DisassemblerElfAArch32::MakeReadAbs32,
       &DisassemblerElfAArch32::MakeWriteAbs32},
      {ReferenceTypeTraits{4, TypeTag(AArch32ReferenceType::kRel32_A24),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch32::MakeReadRel32<
           AArch32Rel32Translator::AddrTraits_A24>,
       &DisassemblerElfAArch32::MakeWriteRel32<
           AArch32Rel32Translator::AddrTraits_A24>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch32Rel32Translator::AddrTraits_A24>},
      {ReferenceTypeTraits{2, TypeTag(AArch32ReferenceType::kRel32_T8),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch32::MakeReadRel32<
           AArch32Rel32Translator::AddrTraits_T8>,
       &DisassemblerElfAArch32::MakeWriteRel32<
           AArch32Rel32Translator::AddrTraits_T8>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch32Rel32Translator::AddrTraits_T8>},
      {ReferenceTypeTraits{2, TypeTag(AArch32ReferenceType::kRel32_T11),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch32::MakeReadRel32<
           AArch32Rel32Translator::AddrTraits_T11>,
       &DisassemblerElfAArch32::MakeWriteRel32<
           AArch32Rel32Translator::AddrTraits_T11>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch32Rel32Translator::AddrTraits_T11>},
      {ReferenceTypeTraits{4, TypeTag(AArch32ReferenceType::kRel32_T20),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch32::MakeReadRel32<
           AArch32Rel32Translator::AddrTraits_T20>,
       &DisassemblerElfAArch32::MakeWriteRel32<
           AArch32Rel32Translator::AddrTraits_T20>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch32Rel32Translator::AddrTraits_T20>},
      {ReferenceTypeTraits{4, TypeTag(AArch32ReferenceType::kRel32_T24),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch32::MakeReadRel32<
           AArch32Rel32Translator::AddrTraits_T24>,
       &DisassemblerElfAArch32::MakeWriteRel32<
           AArch32Rel32Translator::AddrTraits_T24>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch32Rel32Translator::AddrTraits_T24>},
  };
}

std::unique_ptr<DisassemblerElfAArch32::Traits::Rel32FinderUse>
DisassemblerElfAArch32::MakeRel32Finder(
    const typename Traits::Elf_Shdr& section) {
  return std::make_unique<Rel32FinderAArch32>(image_, translator_,
                                              IsExecSectionThumb2(section));
}

bool DisassemblerElfAArch32::IsExecSectionThumb2(
    const typename Traits::Elf_Shdr& section) const {
  // ARM mode requires 4-byte alignment.
  if (section.sh_addr % 4 != 0 || section.sh_size % 4 != 0)
    return true;
  const uint8_t* first = image_.begin() + section.sh_offset;
  const uint8_t* end = first + section.sh_size;
  // Each instruction in 32-bit ARM (little-endian) looks like
  //   ?? ?? ?? X?,
  // where X specifies conditional execution. X = 0xE represents AL = "ALways
  // execute", and tends to appear very often. We use this as our main indicator
  // to discern 32-bit ARM mode from THUMB2 mode.
  size_t num = 0;
  size_t den = 0;
  for (const uint8_t* cur = first; cur < end; cur += 4) {
    // |cur[3]| is within bounds because |end - cur| is a multiple of 4.
    uint8_t maybe_cond = cur[3] & 0xF0;
    if (maybe_cond == 0xE0)
      ++num;
    ++den;
  }

  if (den > 0) {
    LOG(INFO) << "Section scan: " << num << " / " << den << " => "
              << base::StringPrintf("%.2f", num * 100.0 / den) << "%";
  }
  return num < den * kAArch32BitCondAlwaysDensityThreshold;
}

/******** DisassemblerElfAArch64 ********/

DisassemblerElfAArch64::DisassemblerElfAArch64() = default;

DisassemblerElfAArch64::~DisassemblerElfAArch64() = default;

std::vector<ReferenceGroup> DisassemblerElfAArch64::MakeReferenceGroups()
    const {
  return {
      {ReferenceTypeTraits{sizeof(Traits::Elf_Rel::r_offset),
                           TypeTag(AArch64ReferenceType::kReloc),
                           PoolTag(ArmReferencePool::kPoolReloc)},
       &DisassemblerElfAArch64::MakeReadRelocs,
       &DisassemblerElfAArch64::MakeWriteRelocs},
      {ReferenceTypeTraits{Traits::kVAWidth,
                           TypeTag(AArch64ReferenceType::kAbs32),
                           PoolTag(ArmReferencePool::kPoolAbs32)},
       &DisassemblerElfAArch64::MakeReadAbs32,
       &DisassemblerElfAArch64::MakeWriteAbs32},
      {ReferenceTypeTraits{4, TypeTag(AArch64ReferenceType::kRel32_Immd14),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch64::MakeReadRel32<
           AArch64Rel32Translator::AddrTraits_Immd14>,
       &DisassemblerElfAArch64::MakeWriteRel32<
           AArch64Rel32Translator::AddrTraits_Immd14>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch64Rel32Translator::AddrTraits_Immd14>},
      {ReferenceTypeTraits{4, TypeTag(AArch64ReferenceType::kRel32_Immd19),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch64::MakeReadRel32<
           AArch64Rel32Translator::AddrTraits_Immd19>,
       &DisassemblerElfAArch64::MakeWriteRel32<
           AArch64Rel32Translator::AddrTraits_Immd19>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch64Rel32Translator::AddrTraits_Immd19>},
      {ReferenceTypeTraits{4, TypeTag(AArch64ReferenceType::kRel32_Immd26),
                           PoolTag(ArmReferencePool::kPoolRel32)},
       &DisassemblerElfAArch64::MakeReadRel32<
           AArch64Rel32Translator::AddrTraits_Immd26>,
       &DisassemblerElfAArch64::MakeWriteRel32<
           AArch64Rel32Translator::AddrTraits_Immd26>,
       &DisassemblerElfAArch32::MakeMixRel32<
           AArch64Rel32Translator::AddrTraits_Immd26>},
  };
}

std::unique_ptr<DisassemblerElfAArch64::Traits::Rel32FinderUse>
DisassemblerElfAArch64::MakeRel32Finder(
    const typename Traits::Elf_Shdr& section) {
  return std::make_unique<Rel32FinderAArch64>(image_, translator_);
}

// Explicit instantiation for supported classes.
template class DisassemblerElfArm<ElfAArch32Traits>;
template class DisassemblerElfArm<ElfAArch64Traits>;
template bool DisassemblerElf<ElfAArch32Traits>::QuickDetect(
    ConstBufferView image);
template bool DisassemblerElf<ElfAArch64Traits>::QuickDetect(
    ConstBufferView image);

}  // namespace zucchini
