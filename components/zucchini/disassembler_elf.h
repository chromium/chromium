// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/rel32_finder.h"
#include "components/zucchini/rel32_utils.h"
#include "components/zucchini/reloc_elf.h"
#include "components/zucchini/type_elf.h"

namespace zucchini {

struct ArmReferencePool {
  enum : uint8_t {
    kPoolReloc,
    kPoolAbs32,
    kPoolRel32,
  };
};

struct AArch32ReferenceType {
  enum : uint8_t {
    kReloc,  // kPoolReloc

    kAbs32,  // kPoolAbs32

    kRel32_A24,  // kPoolRel32
    kRel32_T8,
    kRel32_T11,
    kRel32_T20,
    kRel32_T24,

    kTypeCount
  };
};

struct AArch64ReferenceType {
  enum : uint8_t {
    kReloc,  // kPoolReloc

    kAbs32,  // kPoolAbs32

    kRel32_Immd14,  // kPoolRel32
    kRel32_Immd19,
    kRel32_Immd26,

    kTypeCount
  };
};

struct Elf32Traits {
  static constexpr uint16_t kVersion = 1;
  static constexpr Bitness kBitness = kBit32;
  static constexpr elf::FileClass kIdentificationClass = elf::ELFCLASS32;
  using Elf_Shdr = elf::Elf32_Shdr;
  using Elf_Phdr = elf::Elf32_Phdr;
  using Elf_Ehdr = elf::Elf32_Ehdr;
  using Elf_Rel = elf::Elf32_Rel;
  using Elf_Rela = elf::Elf32_Rela;
};

// Architecture-specific definitions.

struct Elf32IntelTraits : public Elf32Traits {
  static constexpr ExecutableType kExeType = kExeTypeElfX86;
  static const char kExeTypeString[];
  static constexpr elf::MachineArchitecture kMachineValue = elf::EM_386;
  static constexpr uint32_t kRelType = elf::R_386_RELATIVE;
  enum : uint32_t { kVAWidth = 4 };
  using Rel32FinderUse = Rel32FinderX86;
};

struct ElfAArch32Traits : public Elf32Traits {
  static constexpr ExecutableType kExeType = kExeTypeElfAArch32;
  static const char kExeTypeString[];
  static constexpr elf::MachineArchitecture kMachineValue = elf::EM_ARM;
  static constexpr uint32_t kRelType = elf::R_ARM_RELATIVE;
  enum : uint32_t { kVAWidth = 4 };
  using ArmReferenceType = AArch32ReferenceType;
  using Rel32FinderUse = Rel32FinderAArch32;
};

struct Elf64Traits {
  static constexpr uint16_t kVersion = 1;
  static constexpr Bitness kBitness = kBit64;
  static constexpr elf::FileClass kIdentificationClass = elf::ELFCLASS64;
  using Elf_Shdr = elf::Elf64_Shdr;
  using Elf_Phdr = elf::Elf64_Phdr;
  using Elf_Ehdr = elf::Elf64_Ehdr;
  using Elf_Rel = elf::Elf64_Rel;
  using Elf_Rela = elf::Elf64_Rela;
};

// Architecture-specific definitions.
struct Elf64IntelTraits : public Elf64Traits {
  static constexpr ExecutableType kExeType = kExeTypeElfX64;
  static const char kExeTypeString[];
  static constexpr elf::MachineArchitecture kMachineValue = elf::EM_X86_64;
  static constexpr uint32_t kRelType = elf::R_X86_64_RELATIVE;
  enum : uint32_t { kVAWidth = 8 };
  using Rel32FinderUse = Rel32FinderX64;
};

struct ElfAArch64Traits : public Elf64Traits {
  static constexpr ExecutableType kExeType = kExeTypeElfAArch64;
  static const char kExeTypeString[];
  static constexpr elf::MachineArchitecture kMachineValue = elf::EM_AARCH64;
  // TODO(huangs): See if R_AARCH64_GLOB_DAT and R_AARCH64_JUMP_SLOT should be
  // used.
  static constexpr uint32_t kRelType = elf::R_AARCH64_RELATIVE;
  enum : uint32_t { kVAWidth = 8 };
  using ArmReferenceType = AArch64ReferenceType;
  using Rel32FinderUse = Rel32FinderAArch64;
};

// Decides whether target |offset| is covered by a section in |sorted_headers|.
template <class ELF_SHDR>
bool IsTargetOffsetInElfSectionList(
    const std::vector<const ELF_SHDR*>& sorted_headers,
    offset_t offset) {
  // Use binary search to search in a list of intervals, in a fashion similar to
  // AddressTranslator::OffsetToUnit().
  auto comp = [](offset_t offset, const ELF_SHDR* header) -> bool {
    return offset < header->sh_offset;
  };
  auto it = std::upper_bound(sorted_headers.begin(), sorted_headers.end(),
                             offset, comp);
  if (it == sorted_headers.begin())
    return false;
  --it;
  // Just check offset without worrying about width, since this is a target.
  // Not using RangeCovers() because |sh_offset| and |sh_size| can be 64-bit.
  return offset >= (*it)->sh_offset &&
         offset - (*it)->sh_offset < (*it)->sh_size;
}

// Disassembler for ELF.
template <class TRAITS>
class DisassemblerElf : public Disassembler {
 public:
  using Traits = TRAITS;
  static constexpr uint16_t kVersion = Traits::kVersion;
  // Applies quick checks to determine whether |image| *may* point to the start
  // of an executable. Returns true iff the check passes.
  static bool QuickDetect(ConstBufferView image);

  DisassemblerElf(const DisassemblerElf&) = delete;
  const DisassemblerElf& operator=(const DisassemblerElf&) = delete;
  ~DisassemblerElf() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override = 0;

  // Read/Write functions that are common among different architectures.
  std::unique_ptr<ReferenceReader> MakeReadRelocs(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteRelocs(MutableBufferView image);

  const AddressTranslator& translator() const { return translator_; }

 protected:
  friend Disassembler;

  DisassemblerElf();

  bool Parse(ConstBufferView image) override;

  // Returns the supported Elf_Ehdr::e_machine enum.
  static constexpr elf::MachineArchitecture supported_architecture() {
    return Traits::kMachineValue;
  }

  // Returns the type to look for in the reloc section.
  static constexpr uint32_t supported_relocation_type() {
    return Traits::kRelType;
  }

  // Performs architecture-specific parsing of an executable section, to extract
  // rel32 references.
  virtual void ParseExecSection(const typename Traits::Elf_Shdr& section) = 0;

  // Processes rel32 data after they are extracted from executable sections.
  virtual void PostProcessRel32() = 0;

  // Parses ELF header and section headers, and performs basic validation.
  // Returns whether parsing was successful.
  bool ParseHeader();

  // Extracts and stores section headers that we need.
  void ExtractInterestingSectionHeaders();

  // Parsing functions that extract references from various sections.
  void GetAbs32FromRelocSections();
  void GetRel32FromCodeSections();
  void ParseSections();

  // Main ELF header.
  raw_ptr<const typename Traits::Elf_Ehdr> header_ = nullptr;

  // Section header table, ordered by section id.
  elf::Elf32_Half sections_count_ = 0;
  raw_ptr<const typename Traits::Elf_Shdr, AllowPtrArithmetic> sections_ =
      nullptr;

  // Program header table.
  elf::Elf32_Half segments_count_ = 0;
  raw_ptr<const typename Traits::Elf_Phdr, AllowPtrArithmetic> segments_ =
      nullptr;

  // Bit fields to store the role each section may play.
  std::vector<int> section_judgements_;

  // Translator between offsets and RVAs.
  AddressTranslator translator_;

  // Identity translator for abs32 translation.
  AddressTranslator identity_translator_;

  // Extracted relocation section dimensions data, sorted by file offsets.
  std::vector<SectionDimensionsElf> reloc_section_dims_;

  // Headers of executable sections, sorted by file offsets of the data each
  // header points to.
  std::vector<const typename Traits::Elf_Shdr*> exec_headers_;

  // Sorted file offsets of abs32 locations.
  std::deque<offset_t> abs32_locations_;
};

// Disassembler for ELF with Intel architectures.
template <class TRAITS>
class DisassemblerElfIntel : public DisassemblerElf<TRAITS> {
 public:
  using Traits = TRAITS;
  enum ReferenceType : uint8_t { kReloc, kAbs32, kRel32, kTypeCount };

  DisassemblerElfIntel();
  DisassemblerElfIntel(const DisassemblerElfIntel&) = delete;
  const DisassemblerElfIntel& operator=(const DisassemblerElfIntel&) = delete;
  ~DisassemblerElfIntel() override;

  // Disassembler:
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // DisassemblerElf:
  void ParseExecSection(const typename Traits::Elf_Shdr& section) override;
  void PostProcessRel32() override;

  // Specialized Read/Write functions.
  std::unique_ptr<ReferenceReader> MakeReadAbs32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteAbs32(MutableBufferView image);
  std::unique_ptr<ReferenceReader> MakeReadRel32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteRel32(MutableBufferView image);

 private:
  // Sorted file offsets of rel32 locations.
  // Using std::deque to reduce peak memory footprint.
  std::deque<offset_t> rel32_locations_;
};

using DisassemblerElfX86 = DisassemblerElfIntel<Elf32IntelTraits>;
using DisassemblerElfX64 = DisassemblerElfIntel<Elf64IntelTraits>;

// Disassembler for ELF with ARM architectures.
template <class TRAITS>
class DisassemblerElfArm : public DisassemblerElf<TRAITS> {
 public:
  using Traits = TRAITS;
  DisassemblerElfArm();
  DisassemblerElfArm(const DisassemblerElfArm&) = delete;
  const DisassemblerElfArm& operator=(const DisassemblerElfArm&) = delete;
  ~DisassemblerElfArm() override;

  // Determines whether target |offset| is in an executable section.
  bool IsTargetOffsetInExecSection(offset_t offset) const;

  // Creates an architecture-specific Rel32Finder for ParseExecSection.
  virtual std::unique_ptr<typename Traits::Rel32FinderUse> MakeRel32Finder(
      const typename Traits::Elf_Shdr& section) = 0;

  // DisassemblerElf:
  void ParseExecSection(const typename Traits::Elf_Shdr& section) override;
  void PostProcessRel32() override;

  // Specialized Read/Write functions.
  std::unique_ptr<ReferenceReader> MakeReadAbs32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteAbs32(MutableBufferView image);

  // Specialized Read/Write/Mix functions for different rel32 address types.
  template <class ADDR_TRAITS>
  std::unique_ptr<ReferenceReader> MakeReadRel32(offset_t lower,
                                                 offset_t upper);
  template <class ADDR_TRAITS>
  std::unique_ptr<ReferenceWriter> MakeWriteRel32(MutableBufferView image);

  template <class ADDR_TRAITS>
  std::unique_ptr<ReferenceMixer> MakeMixRel32(ConstBufferView old_image,
                                               ConstBufferView new_image);

 protected:
  // Sorted file offsets of rel32 locations for each rel32 address type.
  std::deque<offset_t>
      rel32_locations_table_[Traits::ArmReferenceType::kTypeCount];
};

// Disassembler for ELF with AArch32 (AKA ARM32).
class DisassemblerElfAArch32 : public DisassemblerElfArm<ElfAArch32Traits> {
 public:
  DisassemblerElfAArch32();
  DisassemblerElfAArch32(const DisassemblerElfAArch32&) = delete;
  const DisassemblerElfAArch32& operator=(const DisassemblerElfAArch32&) =
      delete;
  ~DisassemblerElfAArch32() override;

  // Disassembler:
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // DisassemblerElfArm:
  std::unique_ptr<typename Traits::Rel32FinderUse> MakeRel32Finder(
      const typename Traits::Elf_Shdr& section) override;

  // Under the naive assumption that an executable section is entirely ARM mode
  // or THUMB2 mode, this function implements heuristics to distinguish between
  // the two. Returns true if section is THUMB2 mode; otherwise return false.
  bool IsExecSectionThumb2(const typename Traits::Elf_Shdr& section) const;
};

// Disassembler for ELF with AArch64 (AKA ARM64).
class DisassemblerElfAArch64 : public DisassemblerElfArm<ElfAArch64Traits> {
 public:
  DisassemblerElfAArch64();
  DisassemblerElfAArch64(const DisassemblerElfAArch64&) = delete;
  const DisassemblerElfAArch64& operator=(const DisassemblerElfAArch64&) =
      delete;
  ~DisassemblerElfAArch64() override;

  // Disassembler:
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // DisassemblerElfArm:
  std::unique_ptr<typename Traits::Rel32FinderUse> MakeRel32Finder(
      const typename Traits::Elf_Shdr& section) override;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_
