// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_H_
#define COURGETTE_DISASSEMBLER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "courgette/courgette.h"
#include "courgette/image_utils.h"
#include "courgette/instruction_utils.h"

namespace courgette {

class AssemblyProgram;
class EncodedProgram;

class Disassembler : public AddressTranslator {
 public:
  // Visitor/adaptor to translate RVA to target RVA for abs32.
  class RvaVisitor_Abs32 : public VectorRvaVisitor<RVA> {
   public:
    RvaVisitor_Abs32(const std::vector<RVA>& rva_locations,
                     const AddressTranslator& translator);
    ~RvaVisitor_Abs32() override { }

    // VectorRvaVisitor<RVA> interfaces.
    RVA Get() const override;

   private:
    const AddressTranslator& translator_;

    DISALLOW_COPY_AND_ASSIGN(RvaVisitor_Abs32);
  };

  // Visitor/adaptor to translate RVA to target RVA for rel32.
  class RvaVisitor_Rel32 : public VectorRvaVisitor<RVA> {
   public:
    RvaVisitor_Rel32(const std::vector<RVA>& rva_locations,
                     const AddressTranslator& translator);
    ~RvaVisitor_Rel32() override { }

    // VectorRvaVisitor<RVA> interfaces.
    RVA Get() const override;

   private:
    const AddressTranslator& translator_;

    DISALLOW_COPY_AND_ASSIGN(RvaVisitor_Rel32);
  };

  virtual ~Disassembler();

  // AddressTranslator interfaces.
  RVA FileOffsetToRVA(FileOffset file_offset) const override = 0;
  FileOffset RVAToFileOffset(RVA rva) const override = 0;
  const uint8_t* FileOffsetToPointer(FileOffset file_offset) const override;
  const uint8_t* RVAToPointer(RVA rva) const override;
  RVA PointerToTargetRVA(const uint8_t* p) const override = 0;

  virtual ExecutableType kind() const = 0;

  // Returns the preferred image base address. Using uint64_t to accommodate the
  // general case of 64-bit architectures.
  virtual uint64_t image_base() const = 0;

  // Extracts and stores locations of abs32 references from the image file.
  virtual bool ExtractAbs32Locations() = 0;

  // Extracts and stores locations of rel32 references from the image file.
  virtual bool ExtractRel32Locations() = 0;

  // Returns a caller-owned new RvaVisitor to iterate through abs32 target RVAs.
  virtual RvaVisitor* CreateAbs32TargetRvaVisitor() = 0;

  // Returns a caller-owned new RvaVisitor to iterate through rel32 target RVAs.
  virtual RvaVisitor* CreateRel32TargetRvaVisitor() = 0;

  // Removes unused rel32 locations (architecture-specific). This is needed
  // because we may remove rel32 Labels along the way. As a result the matching
  // rel32 addresses become unused. Removing them saves space.
  virtual void RemoveUnusedRel32Locations(AssemblyProgram* program) = 0;

  // Extracts structural data from the main image. Returns true if the image
  // appears to be a valid executable of the expected type, or false otherwise.
  // This needs to be called before Disassemble().
  virtual bool ParseHeader() = 0;

  // Extracts and stores references from the main image. Returns a new
  // AssemblyProgram with initialized Labels, or null on failure.
  std::unique_ptr<AssemblyProgram> CreateProgram(bool annotate);

  // Goes through the entire program (with the help of |program|), computes all
  // instructions, and stores them into |encoded|.
  Status DisassembleAndEncode(AssemblyProgram* program,
                              EncodedProgram* encoded);

  // ok() may always be called but returns true only after ParseHeader()
  // succeeds.
  bool ok() const { return failure_reason_ == nullptr; }

  // Returns the length of the image. May reduce after ParseHeader().
  size_t length() const { return length_; }
  const uint8_t* start() const { return start_; }
  const uint8_t* end() const { return end_; }

 protected:
  Disassembler(const uint8_t* start, size_t length);

  bool Good();
  bool Bad(const char *reason);

  // Returns true if the given range lies within our memory region.
  bool IsRangeInBounds(size_t offset, size_t size) {
    return offset <= length() && size <= length() - offset;
  }

  // Returns true if the given array lies within our memory region.
  bool IsArrayInBounds(size_t offset, size_t elements, size_t element_size) {
    return offset <= length() && elements <= (length() - offset) / element_size;
  }

  // Computes and stores all Labels before scanning program bytes.
  void PrecomputeLabels(AssemblyProgram* program);

  // Reduce the length of the image in memory. Does not actually free
  // (or realloc) any memory. Usually only called via ParseHeader().
  void ReduceLength(size_t reduced_length);

  // Returns a generator that emits instructions to a given receptor. |program|
  // is required as helper.
  virtual InstructionGenerator GetInstructionGenerator(
      AssemblyProgram* program) = 0;

 private:
  const char* failure_reason_;

  //
  // Basic information that is always valid after construction, although
  // ParseHeader() may shorten |length_| if the executable is shorter than the
  // total data.
  //
  size_t length_;         // In current memory.
  const uint8_t* start_;  // In current memory, base for 'file offsets'.
  const uint8_t* end_;    // In current memory.

  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_H_
