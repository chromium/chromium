// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_H_
#define COURGETTE_DISASSEMBLER_ELF_32_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "courgette/disassembler.h"
#include "courgette/image_utils.h"
#include "courgette/instruction_utils.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

class AssemblyProgram;

// A Courgette disassembler for 32-bit ELF files. This is only a partial
// implementation that admits subclasses for the architecture-specific parts of
// 32-bit ELF file processing. Specifically:
// - RelToRVA() processes entries in ELF relocation table.
// - ParseRelocationSection() verifies the organization of the ELF relocation
//   table.
// - ParseRel32RelocsFromSection() finds branch targets by looking for relative
//   branch/call opcodes in the particular architecture's machine code.
class DisassemblerElf32 : public Disassembler {
 public:
  // Different instructions encode the target rva differently.  This
  // class encapsulates this behavior.  public for use in unit tests.
  class TypedRVA {
   public:
    explicit TypedRVA(RVA rva) : rva_(rva) { }

    virtual ~TypedRVA() { }

    RVA rva() const { return rva_; }
    RVA relative_target() const { return relative_target_; }
    FileOffset file_offset() const { return file_offset_; }

    void set_relative_target(RVA relative_target) {
      relative_target_ = relative_target;
    }
    void set_file_offset(FileOffset file_offset) { file_offset_ = file_offset; }

    // Computes the relative jump's offset from the op in p.
    virtual CheckBool ComputeRelativeTarget(const uint8_t* op_pointer) = 0;

    // Emits the assembly instruction corresponding to |label|.
    virtual CheckBool EmitInstruction(Label* label,
                                      InstructionReceptor* receptor) = 0;

    // Returns the size of the instruction containing the RVA.
    virtual uint16_t op_size() const = 0;

    // Comparator for sorting, which assumes uniqueness of RVAs.
    static bool IsLessThanByRVA(const std::unique_ptr<TypedRVA>& a,
                                const std::unique_ptr<TypedRVA>& b) {
      return a->rva() < b->rva();
    }

    // Comparator for sorting, which assumes uniqueness of file offsets.
    static bool IsLessThanByFileOffset(const std::unique_ptr<TypedRVA>& a,
                                       const std::unique_ptr<TypedRVA>& b) {
      return a->file_offset() < b->file_offset();
    }

   private:
    const RVA rva_;
    RVA relative_target_ = kNoRVA;
    FileOffset file_offset_ = kNoFileOffset;
  };

  // Visitor/adaptor to translate RVA to target RVA. This is the ELF
  // counterpart to RvaVisitor_Rel32 that uses TypedRVA.
  class Elf32RvaVisitor_Rel32 :
  public VectorRvaVisitor<std::unique_ptr<TypedRVA>> {
   public:
    Elf32RvaVisitor_Rel32(
        const std::vector<std::unique_ptr<TypedRVA>>& rva_locations);
    ~Elf32RvaVisitor_Rel32() override { }

    // VectorRvaVisitor<TypedRVA*> interfaces.
    RVA Get() const override;

   private:
    DISALLOW_COPY_AND_ASSIGN(Elf32RvaVisitor_Rel32);
  };

 public:
  DisassemblerElf32(const uint8_t* start, size_t length);

  ~DisassemblerElf32() override { }

  // Disassembler interfaces.
  RVA FileOffsetToRVA(FileOffset file_offset) const override;
  FileOffset RVAToFileOffset(RVA rva) const override;
  RVA PointerToTargetRVA(const uint8_t* p) const override;
  ExecutableType kind() const override = 0;
  uint64_t image_base() const override { return 0; }
  bool ParseHeader() override;

  virtual e_machine_values ElfEM() const = 0;

  CheckBool IsValidTargetRVA(RVA rva) const WARN_UNUSED_RESULT;

  // Converts an ELF relocation instruction into an RVA.
  virtual CheckBool RelToRVA(Elf32_Rel rel, RVA* result)
    const WARN_UNUSED_RESULT = 0;

  // Public for unittests only
  std::vector<RVA>& Abs32Locations() { return abs32_locations_; }
  std::vector<std::unique_ptr<TypedRVA>>& Rel32Locations() {
    return rel32_locations_;
  }

 protected:
  // Returns 'true' if an valid executable is detected using only quick checks.
  // Derived classes should inject |elf_em| corresponding to their architecture,
  // which will be checked against the detected one.
  static bool QuickDetect(const uint8_t* start,
                          size_t length,
                          e_machine_values elf_em);

  // Returns whether all non-SHT_NOBITS sections lie within image.
  bool CheckSectionRanges();

  // Returns whether all program segments lie within image.
  bool CheckProgramSegmentRanges();

  void UpdateLength();

  // Misc Section Helpers

  Elf32_Half SectionHeaderCount() const {
    return section_header_table_size_;
  }

  const Elf32_Shdr* SectionHeader(Elf32_Half id) const {
    assert(id >= 0 && id < SectionHeaderCount());
    return &section_header_table_[id];
  }

  const uint8_t* SectionBody(Elf32_Half id) const {
    const Elf32_Shdr* section_header = SectionHeader(id);
    DCHECK(section_header->sh_type != SHT_NOBITS);
    return FileOffsetToPointer(section_header->sh_offset);
  }

  // Gets the |name| of section |shdr|. Returns true on success.
  CheckBool SectionName(const Elf32_Shdr& shdr, std::string* name) const;

  // Misc Segment Helpers

  Elf32_Half ProgramSegmentHeaderCount() const {
    return program_header_table_size_;
  }

  const Elf32_Phdr* ProgramSegmentHeader(Elf32_Half id) const {
    assert(id >= 0 && id < ProgramSegmentHeaderCount());
    return program_header_table_ + id;
  }

  // Misc address space helpers

  CheckBool RVAsToFileOffsets(const std::vector<RVA>& rvas,
                              std::vector<FileOffset>* file_offsets) const;

  CheckBool RVAsToFileOffsets(
      std::vector<std::unique_ptr<TypedRVA>>* typed_rvas) const;

  // Helpers for ParseFile().

  virtual CheckBool ParseRelocationSection(const Elf32_Shdr* section_header,
                                           InstructionReceptor* receptor) const
      WARN_UNUSED_RESULT = 0;

  virtual CheckBool ParseRel32RelocsFromSection(const Elf32_Shdr* section)
      WARN_UNUSED_RESULT = 0;

  CheckBool ParseAbs32Relocs() WARN_UNUSED_RESULT;

  // Extracts all rel32 TypedRVAs. Does not sort the result.
  CheckBool ParseRel32RelocsFromSections() WARN_UNUSED_RESULT;

  // Disassembler interfaces.
  bool ExtractAbs32Locations() override;
  bool ExtractRel32Locations() override;
  RvaVisitor* CreateAbs32TargetRvaVisitor() override;
  RvaVisitor* CreateRel32TargetRvaVisitor() override;
  void RemoveUnusedRel32Locations(AssemblyProgram* program) override;
  InstructionGenerator GetInstructionGenerator(
      AssemblyProgram* program) override;

  CheckBool ParseFile(AssemblyProgram* target,
                      InstructionReceptor* receptor) const WARN_UNUSED_RESULT;

  CheckBool ParseProgbitsSection(
      const Elf32_Shdr* section_header,
      std::vector<FileOffset>::iterator* current_abs_offset,
      std::vector<FileOffset>::iterator end_abs_offset,
      std::vector<std::unique_ptr<TypedRVA>>::iterator* current_rel,
      std::vector<std::unique_ptr<TypedRVA>>::iterator end_rel,
      AssemblyProgram* program,
      InstructionReceptor* receptor) const WARN_UNUSED_RESULT;

  CheckBool ParseSimpleRegion(FileOffset start_file_offset,
                              FileOffset end_file_offset,
                              InstructionReceptor* receptor) const
      WARN_UNUSED_RESULT;

  CheckBool CheckSection(RVA rva) WARN_UNUSED_RESULT;

  const Elf32_Ehdr* header_;

  Elf32_Half section_header_table_size_;

  // Section header table, ordered by section id.
  std::vector<Elf32_Shdr> section_header_table_;

  // An ordering of |section_header_table_|, sorted by file offset.
  std::vector<Elf32_Half> section_header_file_offset_order_;

  const Elf32_Phdr* program_header_table_;
  Elf32_Half program_header_table_size_;

  // Pointer to string table containing section names.
  const char* default_string_section_;
  size_t default_string_section_size_;

  // Sorted abs32 RVAs.
  std::vector<RVA> abs32_locations_;
  // Sorted rel32 RVAs. This is mutable because ParseFile() temporarily sorts
  // these by file offsets.
  mutable std::vector<std::unique_ptr<TypedRVA>> rel32_locations_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_H_
