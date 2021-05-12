// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/rel32_finder.h"
#include "components/zucchini/rel32_utils.h"
#include "components/zucchini/reloc_elf.h"
#include "components/zucchini/type_elf.h"

namespace zucchini {

struct Elf32Traits {
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

struct Elf64Traits {
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

// Disassembler for ELF.
template <class Traits>
class DisassemblerElf : public Disassembler {
 public:
  // Applies quick checks to determine whether |image| *may* point to the start
  // of an executable. Returns true iff the check passes.
  static bool QuickDetect(ConstBufferView image);

  ~DisassemblerElf() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override = 0;

  // Find/Receive functions that are common among different architectures.
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
  const typename Traits::Elf_Ehdr* header_ = nullptr;

  // Section header table, ordered by section id.
  elf::Elf32_Half sections_count_ = 0;
  const typename Traits::Elf_Shdr* sections_ = nullptr;

  // Program header table.
  elf::Elf32_Half segments_count_ = 0;
  const typename Traits::Elf_Phdr* segments_ = nullptr;

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
  std::vector<offset_t> abs32_locations_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf);
};

// Disassembler for ELF with Intel architectures.
template <class Traits>
class DisassemblerElfIntel : public DisassemblerElf<Traits> {
 public:
  enum ReferenceType : uint8_t { kReloc, kAbs32, kRel32, kTypeCount };

  DisassemblerElfIntel();
  ~DisassemblerElfIntel() override;

  // Disassembler:
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // DisassemblerElf:
  void ParseExecSection(const typename Traits::Elf_Shdr& section) override;
  void PostProcessRel32() override;

  // Specialized Find/Receive functions.
  std::unique_ptr<ReferenceReader> MakeReadAbs32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteAbs32(MutableBufferView image);
  std::unique_ptr<ReferenceReader> MakeReadRel32(offset_t lo, offset_t hi);
  std::unique_ptr<ReferenceWriter> MakeWriteRel32(MutableBufferView image);

 private:
  // Sorted file offsets of rel32 locations.
  std::vector<offset_t> rel32_locations_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElfIntel);
};

using DisassemblerElfX86 = DisassemblerElfIntel<Elf32IntelTraits>;
using DisassemblerElfX64 = DisassemblerElfIntel<Elf64IntelTraits>;

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_ELF_H_
