// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_WIN32_H_
#define COURGETTE_DISASSEMBLER_WIN32_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "courgette/disassembler.h"
#include "courgette/image_utils.h"
#include "courgette/instruction_utils.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_win_pe.h"

namespace courgette {

class AssemblyProgram;

class DisassemblerWin32 : public Disassembler {
 public:
  virtual ~DisassemblerWin32() = default;

  // Disassembler interfaces.
  RVA FileOffsetToRVA(FileOffset file_offset) const override;
  FileOffset RVAToFileOffset(RVA rva) const override;
  ExecutableType kind() const override = 0;
  uint64_t image_base() const override { return image_base_; }
  RVA PointerToTargetRVA(const uint8_t* p) const override = 0;
  bool ParseHeader() override;

  // Exposed for test purposes
  bool has_text_section() const { return has_text_section_; }
  uint32_t size_of_code() const { return size_of_code_; }

  // Returns 'true' if the base relocation table can be parsed.
  // Output is a vector of the RVAs corresponding to locations within executable
  // that are listed in the base relocation table.
  bool ParseRelocs(std::vector<RVA>* addresses);

  // Returns Section containing the relative virtual address, or null if none.
  const Section* RVAToSection(RVA rva) const;

  static std::string SectionName(const Section* section);

 protected:
  // Returns true if a valid executable is detected using only quick checks.
  // Derived classes should inject |magic| corresponding to their architecture,
  // which will be checked against the detected one.
  static bool QuickDetect(const uint8_t* start, size_t length, uint16_t magic);

  // Returns true if the given RVA range lies within [0, |size_of_image_|).
  bool IsRvaRangeInBounds(size_t start, size_t length);

  // Returns whether all sections lie within image.
  bool CheckSectionRanges();

  // Disassembler interfaces.
  bool ExtractAbs32Locations() override;
  bool ExtractRel32Locations() override;
  RvaVisitor* CreateAbs32TargetRvaVisitor() override;
  RvaVisitor* CreateRel32TargetRvaVisitor() override;
  void RemoveUnusedRel32Locations(AssemblyProgram* program) override;
  InstructionGenerator GetInstructionGenerator(
      AssemblyProgram* program) override;

  DisassemblerWin32(const uint8_t* start, size_t length);

  CheckBool ParseFile(AssemblyProgram* target,
                      InstructionReceptor* receptor) const WARN_UNUSED_RESULT;
  virtual void ParseRel32RelocsFromSection(const Section* section) = 0;

  CheckBool ParseNonSectionFileRegion(FileOffset start_file_offset,
                                      FileOffset end_file_offset,
                                      InstructionReceptor* receptor) const
      WARN_UNUSED_RESULT;
  CheckBool ParseFileRegion(const Section* section,
                            FileOffset start_file_offset,
                            FileOffset end_file_offset,
                            AssemblyProgram* program,
                            InstructionReceptor* receptor) const
      WARN_UNUSED_RESULT;

  // Returns address width in byte count.
  virtual int AbsVAWidth() const = 0;
  // Emits Abs 32/64 |label| to the |receptor|.
  virtual CheckBool EmitAbs(Label* label,
                            InstructionReceptor* receptor) const = 0;
  // Returns true if type is recognized.
  virtual bool SupportsRelTableType(int type) const = 0;
  virtual uint16_t RelativeOffsetOfDataDirectories() const = 0;

#if COURGETTE_HISTOGRAM_TARGETS
  void HistogramTargets(const char* kind, const std::map<RVA, int>& map) const;
#endif

  const ImageDataDirectory& base_relocation_table() const {
    return base_relocation_table_;
  }

  // Returns description of the RVA, e.g. ".text+0x1243". For debugging only.
  std::string DescribeRVA(RVA rva) const;

  // Finds the first section at file_offset or above. Does not return sections
  // that have no raw bytes in the file.
  const Section* FindNextSection(FileOffset file_offset) const;

  bool ReadDataDirectory(int index, ImageDataDirectory* dir);

  bool incomplete_disassembly_ =
      false;  // true if can omit "uninteresting" bits.

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

  // Location and size of IMAGE_OPTIONAL_HEADER in the buffer.
  const uint8_t* optional_header_ = nullptr;
  uint16_t size_of_optional_header_ = 0;

  uint16_t machine_type_ = 0;
  uint16_t number_of_sections_ = 0;
  const Section* sections_ = nullptr;
  bool has_text_section_ = false;

  uint32_t size_of_code_ = 0;
  uint32_t size_of_initialized_data_ = 0;
  uint32_t size_of_uninitialized_data_ = 0;
  RVA base_of_code_ = 0;
  RVA base_of_data_ = 0;

  uint64_t image_base_ = 0;  // Range limited to 32 bits for 32 bit executable.
  // Specifies size of loaded PE in memory, and provides bound on RVA.
  uint32_t size_of_image_ = 0;
  int number_of_data_directories_ = 0;

  ImageDataDirectory export_table_;
  ImageDataDirectory import_table_;
  ImageDataDirectory resource_table_;
  ImageDataDirectory exception_table_;
  ImageDataDirectory base_relocation_table_;
  ImageDataDirectory bound_import_table_;
  ImageDataDirectory import_address_table_;
  ImageDataDirectory delay_import_descriptor_;
  ImageDataDirectory clr_runtime_header_;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> abs32_target_rvas_;
  std::map<RVA, int> rel32_target_rvas_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassemblerWin32);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_WIN32_H_
