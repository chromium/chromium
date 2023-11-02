// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_no_op.h"

namespace zucchini {

// |num_equivalence_iterations_| = 1 since no pointers are present.
DisassemblerNoOp::DisassemblerNoOp() : Disassembler(1) {}

DisassemblerNoOp::~DisassemblerNoOp() = default;

ExecutableType DisassemblerNoOp::GetExeType() const {
  return kExeTypeNoOp;
}

std::string DisassemblerNoOp::GetExeTypeString() const {
  return "(Unknown)";
}

std::vector<ReferenceGroup> DisassemblerNoOp::MakeReferenceGroups() const {
  return std::vector<ReferenceGroup>();
}

bool DisassemblerNoOp::Parse(ConstBufferView image) {
  image_ = image;
  return true;
}

}  // namespace zucchini
