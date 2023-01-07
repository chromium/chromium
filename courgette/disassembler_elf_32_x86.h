// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_X86_H_
#define COURGETTE_DISASSEMBLER_ELF_32_X86_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "courgette/disassembler_elf_32.h"
#include "courgette/types_elf.h"

namespace courgette {

class InstructionReceptor;

class DisassemblerElf32X86 : public DisassemblerElf32 {
 public:
  // Returns true if a valid executable is detected using only quick checks.
  static bool QuickDetect(const uint8_t* start, size_t length) {
    return DisassemblerElf32::QuickDetect(start, length, EM_386);
  }

  class TypedRVAX86 : public TypedRVA {
   public:
    explicit TypedRVAX86(RVA rva) : TypedRVA(rva) { }
    ~TypedRVAX86() override { }

    // TypedRVA interfaces.
    CheckBool ComputeRelativeTarget(const uint8_t* op_pointer) override;
    CheckBool EmitInstruction(Label* label,
                              InstructionReceptor* receptor) override;
    uint16_t op_size() const override;
  };

  DisassemblerElf32X86(const uint8_t* start, size_t length);

  DisassemblerElf32X86(const DisassemblerElf32X86&) = delete;
  DisassemblerElf32X86& operator=(const DisassemblerElf32X86&) = delete;

  ~DisassemblerElf32X86() override { }

  // DisassemblerElf32 interfaces.
  ExecutableType kind() const override { return EXE_ELF_32_X86; }
  e_machine_values ElfEM() const override { return EM_386; }

 protected:
  // DisassemblerElf32 interfaces.
  [[nodiscard]] CheckBool RelToRVA(Elf32_Rel rel, RVA* result) const override;
  [[nodiscard]] CheckBool ParseRelocationSection(
      const Elf32_Shdr* section_header,
      InstructionReceptor* receptor) const override;
  [[nodiscard]] CheckBool ParseRel32RelocsFromSection(
      const Elf32_Shdr* section) override;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> rel32_target_rvas_;
#endif
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_X86_H_
