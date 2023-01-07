// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ENCODED_PROGRAM_H_
#define COURGETTE_ENCODED_PROGRAM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "courgette/courgette.h"
#include "courgette/image_utils.h"
#include "courgette/instruction_utils.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

// Stream indexes.
const int kStreamMisc = 0;
const int kStreamOps = 1;
const int kStreamBytes = 2;
const int kStreamAbs32Indexes = 3;
const int kStreamRel32Indexes = 4;
const int kStreamAbs32Addresses = 5;
const int kStreamRel32Addresses = 6;
const int kStreamCopyCounts = 7;
const int kStreamOriginAddresses = kStreamMisc;

const int kStreamLimit = 9;

class LabelManager;
class SinkStream;
class SinkStreamSet;
class SourceStreamSet;

// EncodedProgram encodes Courgette's simple "binary assembly language", which
// can be assembled to produce a sequence of bytes (e.g., a Windows 32-bit
// executable).
class EncodedProgram {
 public:
  EncodedProgram();

  EncodedProgram(const EncodedProgram&) = delete;
  EncodedProgram& operator=(const EncodedProgram&) = delete;

  ~EncodedProgram();

  // Generating an EncodedProgram:
  //
  // (1) The image base can be specified at any time.
  void set_image_base(uint64_t base) { image_base_ = base; }

  // (2) Address tables and indexes imported first.

  [[nodiscard]] CheckBool ImportLabels(const LabelManager& abs32_label_manager,
                                       const LabelManager& rel32_label_manager);

  // (3) Add instructions in the order needed to generate bytes of file.
  // NOTE: If any of these methods ever fail, the EncodedProgram instance
  // has failed and should be discarded.
  [[nodiscard]] CheckBool AddOrigin(RVA rva);
  [[nodiscard]] CheckBool AddCopy(size_t count, const void* bytes);
  [[nodiscard]] CheckBool AddRel32(int label_index);
  [[nodiscard]] CheckBool AddAbs32(int label_index);
  [[nodiscard]] CheckBool AddAbs64(int label_index);
  [[nodiscard]] CheckBool AddPeMakeRelocs(ExecutableType kind);
  [[nodiscard]] CheckBool AddElfMakeRelocs();

  // (3) Serialize binary assembly language tables to a set of streams.
  [[nodiscard]] CheckBool WriteTo(SinkStreamSet* streams);

  // Using an EncodedProgram to generate a byte stream:
  //
  // (4) Deserializes a fresh EncodedProgram from a set of streams.
  bool ReadFrom(SourceStreamSet* streams);

  // (5) Assembles the 'binary assembly language' into final file.
  [[nodiscard]] CheckBool AssembleTo(SinkStream* buffer);

  // Calls |gen| to extract all instructions, which are then encoded and stored.
  CheckBool GenerateInstructions(ExecutableType exe_type,
                                 const InstructionGenerator& gen);

 private:
  // Binary assembly language operations.
  // These are part of the patch format. Reusing an existing value will
  // break backwards compatibility.
  enum OP {
    ORIGIN = 0,  // ORIGIN <rva> - set address for subsequent assembly.
    COPY = 1,    // COPY <count> <bytes> - copy bytes to output.
    COPY1 = 2,   // COPY1 <byte> - same as COPY 1 <byte>.
    REL32 = 3,   // REL32 <index> - emit rel32 encoded reference to address at
                 // address table offset <index>.
    ABS32 = 4,   // ABS32 <index> - emit abs32 encoded reference to address at
                 // address table offset <index>.
    MAKE_PE_RELOCATION_TABLE = 5,   // Emit PE base relocation table blocks.
    MAKE_ELF_RELOCATION_TABLE = 6,  // Emit ELF relocation table for X86.
    // DEPCREATED: ELF relocation table for ARM.
    // MAKE_ELF_ARM_RELOCATION_TABLE_DEPRECATED = 7,
    MAKE_PE64_RELOCATION_TABLE = 8,  // Emit PE64 base relocation table blocks.
    ABS64 = 9,  // ABS64 <index> - emit abs64 encoded reference to address at
                // address table offset <index>.
  };

  typedef NoThrowBuffer<RVA> RvaVector;
  typedef NoThrowBuffer<size_t> SizeTVector;
  typedef NoThrowBuffer<uint32_t> UInt32Vector;
  typedef NoThrowBuffer<uint8_t> UInt8Vector;
  typedef NoThrowBuffer<OP> OPVector;

  void DebuggingSummary();

  // Helper for ImportLabels().
  static CheckBool WriteRvasToList(const LabelManager& label_manager,
                                   RvaVector* rvas);

  // Helper for ImportLabels().
  static void FillUnassignedRvaSlots(RvaVector* rvas);

  [[nodiscard]] CheckBool GeneratePeRelocations(SinkStream* buffer,
                                                uint8_t type);
  [[nodiscard]] CheckBool GenerateElfRelocations(
      Elf32_Word pending_elf_relocation_table,
      SinkStream* buffer);

  // Binary assembly language tables.
  uint64_t image_base_ = 0;
  RvaVector rel32_rva_;
  RvaVector abs32_rva_;
  OPVector ops_;
  RvaVector origins_;
  SizeTVector copy_counts_;
  UInt8Vector copy_bytes_;
  UInt32Vector rel32_ix_;
  UInt32Vector abs32_ix_;

  // Table of the addresses containing abs32 relocations; computed during
  // assembly, used to generate base relocation table.
  UInt32Vector abs32_relocs_;
};

// Deserializes program from a stream set to |*output|. Returns C_OK if
// successful, otherwise assigns |*output| to null and returns an error status.
Status ReadEncodedProgram(SourceStreamSet* source,
                          std::unique_ptr<EncodedProgram>* output);

}  // namespace courgette

#endif  // COURGETTE_ENCODED_PROGRAM_H_
