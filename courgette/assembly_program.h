// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ASSEMBLY_PROGRAM_H_
#define COURGETTE_ASSEMBLY_PROGRAM_H_

#include <stdint.h>

#include <vector>

#include "courgette/courgette.h"
#include "courgette/image_utils.h"
#include "courgette/instruction_utils.h"
#include "courgette/label_manager.h"
#include "courgette/memory_allocator.h"  // For CheckBool.

namespace courgette {

class EncodedProgram;

// An AssemblyProgram stores Labels extracted from an executable file, and
// (optionally) Label annotations. It is initialized by a Disassembler, but
// stores separate state so that the Disassembler can be deleted. Typical usage:
//
// * The Disassembler calls PrecomputeLabels() and injects RVAs for abs32/rel32
//   references. These are used to initialize labels.
// * The Disassembler calls DefaultAssignIndexes() to assign addresses to
//   positions in the address tables.
// * [Optional step]
// * The Disassembler can use Labels in AssemblyProgram to convert the
//   executable file to an EncodedProgram, serialized to an output stream.
// * Later, the Disassembler can use the AssemblyProgram to can be deserialized
//   and assembled into the original executable file via an EncodedProgram.
//
// The optional step is to adjust Labels in the AssemblyProgram. One form of
// adjustment is to assign indexes in such a way as to make the EncodedProgram
// for an executable look more like the EncodedProgram for another exectuable.
// The adjustment process should call UnassignIndexes(), do its own assignment,
// and then call AssignRemainingIndexes() to ensure all indexes are assigned.
class AssemblyProgram {
 public:
  AssemblyProgram(ExecutableType kind, uint64_t image_base);

  AssemblyProgram(const AssemblyProgram&) = delete;
  AssemblyProgram& operator=(const AssemblyProgram&) = delete;

  ~AssemblyProgram();

  ExecutableType kind() const { return kind_; }
  const std::vector<Label*>& abs32_label_annotations() const {
    return abs32_label_annotations_;
  }
  const std::vector<Label*>& rel32_label_annotations() const {
    return rel32_label_annotations_;
  }

  // Traverses RVAs in |abs32_visitor| and |rel32_visitor| to precompute Labels.
  void PrecomputeLabels(RvaVisitor* abs32_visitor, RvaVisitor* rel32_visitor);

  void UnassignIndexes();
  void DefaultAssignIndexes();
  void AssignRemainingIndexes();

  // Looks up abs32 label. Returns null if none found.
  Label* FindAbs32Label(RVA rva);

  // Looks up rel32 label. Returns null if none found.
  Label* FindRel32Label(RVA rva);

  // Uses |gen| to initializes |*_label_annotations_|.
  CheckBool AnnotateLabels(const InstructionGenerator& gen);

  // Initializes |encoded| by injecting basic data and Label data.
  bool PrepareEncodedProgram(EncodedProgram* encoded) const;

 private:
  static const int kLabelLowerLimit;

  // Looks up a label or creates a new one.  Might return nullptr.
  Label* FindLabel(RVA rva, RVAToLabel* labels);

  const ExecutableType kind_;
  const uint64_t image_base_;  // Desired or mandated base address of image.

  // Storage and lookup of Labels associated with target addresses. We use
  // separate abs32 and rel32 labels.
  LabelManager abs32_label_manager_;
  LabelManager rel32_label_manager_;

  // Label pointers for each abs32 and rel32 location, sorted by file offset.
  // These are used by Label adjustment during patch generation.
  std::vector<Label*> abs32_label_annotations_;
  std::vector<Label*> rel32_label_annotations_;
};

}  // namespace courgette

#endif  // COURGETTE_ASSEMBLY_PROGRAM_H_
