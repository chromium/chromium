// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "courgette/assembly_program.h"
#include "courgette/encoded_program.h"

namespace courgette {

Disassembler::RvaVisitor_Abs32::RvaVisitor_Abs32(
    const std::vector<RVA>& rva_locations,
    const AddressTranslator& translator)
    : VectorRvaVisitor<RVA>(rva_locations), translator_(translator) {
}

RVA Disassembler::RvaVisitor_Abs32::Get() const {
  // For Abs32 targets, get target RVA from architecture-dependent functions.
  return translator_.PointerToTargetRVA(translator_.RVAToPointer(*it_));
}

Disassembler::RvaVisitor_Rel32::RvaVisitor_Rel32(
    const std::vector<RVA>& rva_locations,
    const AddressTranslator& translator)
    : VectorRvaVisitor<RVA>(rva_locations), translator_(translator) {
}

RVA Disassembler::RvaVisitor_Rel32::Get() const {
  // For Rel32 targets, only handle 32-bit offsets.
  return *it_ + 4 + Read32LittleEndian(translator_.RVAToPointer(*it_));
}

Disassembler::Disassembler(const uint8_t* start, size_t length)
    : failure_reason_("uninitialized") {
  start_ = start;
  length_ = length;
  end_ = start_ + length_;
}

Disassembler::~Disassembler() = default;

const uint8_t* Disassembler::FileOffsetToPointer(FileOffset file_offset) const {
  CHECK_LE(file_offset, static_cast<FileOffset>(end_ - start_));
  return start_ + file_offset;
}

const uint8_t* Disassembler::RVAToPointer(RVA rva) const {
  FileOffset file_offset = RVAToFileOffset(rva);
  if (file_offset == kNoFileOffset)
    return nullptr;

  return FileOffsetToPointer(file_offset);
}

std::unique_ptr<AssemblyProgram> Disassembler::CreateProgram(bool annotate) {
  if (!ok() || !ExtractAbs32Locations() || !ExtractRel32Locations())
    return nullptr;

  std::unique_ptr<AssemblyProgram> program =
      std::make_unique<AssemblyProgram>(kind(), image_base());

  PrecomputeLabels(program.get());
  RemoveUnusedRel32Locations(program.get());
  program->DefaultAssignIndexes();

  if (annotate) {
    if (!program->AnnotateLabels(GetInstructionGenerator(program.get())))
      return nullptr;
  }

  return program;
}

Status Disassembler::DisassembleAndEncode(AssemblyProgram* program,
                                          EncodedProgram* encoded) {
  program->PrepareEncodedProgram(encoded);
  return encoded->GenerateInstructions(program->kind(),
                                       GetInstructionGenerator(program))
             ? C_OK
             : C_DISASSEMBLY_FAILED;
}

bool Disassembler::Good() {
  failure_reason_ = nullptr;
  return true;
}

bool Disassembler::Bad(const char* reason) {
  failure_reason_ = reason;
  return false;
}

void Disassembler::PrecomputeLabels(AssemblyProgram* program) {
  std::unique_ptr<RvaVisitor> abs32_visitor(CreateAbs32TargetRvaVisitor());
  std::unique_ptr<RvaVisitor> rel32_visitor(CreateRel32TargetRvaVisitor());
  program->PrecomputeLabels(abs32_visitor.get(), rel32_visitor.get());
}

void Disassembler::ReduceLength(size_t reduced_length) {
  CHECK_LE(reduced_length, length_);
  length_ = reduced_length;
  end_ = start_ + length_;
}

}  // namespace courgette
