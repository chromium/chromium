// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/codegen.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "sandbox/linux/system_headers/linux_filter.h"

// This CodeGen implementation strives for simplicity while still
// generating acceptable BPF programs under typical usage patterns
// (e.g., by PolicyCompiler).
//
// The key to its simplicity is that BPF programs only support forward
// jumps/branches, which allows constraining the DAG construction API
// to make instruction nodes immutable. Immutable nodes admits a
// simple greedy approach of emitting new instructions as needed and
// then reusing existing ones that have already been emitted. This
// cleanly avoids any need to compute basic blocks or apply
// topological sorting because the API effectively sorts instructions
// for us (e.g., before MakeInstruction() can be called to emit a
// branch instruction, it must have already been called for each
// branch path).
//
// This greedy algorithm is not without (theoretical) weakness though:
//
//   1. In the general case, we don't eliminate dead code.  If needed,
//      we could trace back through the program in Compile() and elide
//      any unneeded instructions, but in practice we only emit live
//      instructions anyway.
//
//   2. By not dividing instructions into basic blocks and sorting, we
//      lose an opportunity to move non-branch/non-return instructions
//      adjacent to their successor instructions, which means we might
//      need to emit additional jumps. But in practice, they'll
//      already be nearby as long as callers don't go out of their way
//      to interleave MakeInstruction() calls for unrelated code
//      sequences.

namespace sandbox {

// kBranchRange is the maximum value that can be stored in
// sock_filter's 8-bit jt and jf fields.
const size_t kBranchRange = std::numeric_limits<uint8_t>::max();

const CodeGen::Node CodeGen::kNullNode;

CodeGen::CodeGen() : program_(), equivalent_(), memos_() {
}

CodeGen::~CodeGen() {
}

CodeGen::Program CodeGen::Compile(CodeGen::Node head) {
  return Program(program_.rbegin() + Offset(head), program_.rend());
}

CodeGen::Node CodeGen::MakeInstruction(uint16_t code,
                                       uint32_t k,
                                       Node jt,
                                       Node jf) {
  // To avoid generating redundant code sequences, we memoize the
  // results from AppendInstruction().
  auto res = memos_.insert(std::make_pair(MemoKey(code, k, jt, jf), kNullNode));
  CodeGen::Node* node = &res.first->second;
  if (res.second) {  // Newly inserted memo entry.
    *node = AppendInstruction(code, k, jt, jf);
  }
  return *node;
}

CodeGen::Node CodeGen::AppendInstruction(uint16_t code,
                                         uint32_t k,
                                         Node jt,
                                         Node jf) {
  if (BPF_CLASS(code) == BPF_JMP) {
    CHECK_NE(BPF_JA, BPF_OP(code)) << "CodeGen inserts JAs as needed";

    // Optimally adding jumps is rather tricky, so we use a quick
    // approximation: by artificially reducing |jt|'s range, |jt| will
    // stay within its true range even if we add a jump for |jf|.
    jt = WithinRange(jt, kBranchRange - 1);
    jf = WithinRange(jf, kBranchRange);
    return Append(code, k, Offset(jt), Offset(jf));
  }

  CHECK_EQ(kNullNode, jf) << "Non-branch instructions shouldn't provide jf";
  if (BPF_CLASS(code) == BPF_RET) {
    CHECK_EQ(kNullNode, jt) << "Return instructions shouldn't provide jt";
  } else {
    // For non-branch/non-return instructions, execution always
    // proceeds to the next instruction; so we need to arrange for
    // that to be |jt|.
    jt = WithinRange(jt, 0);
    CHECK_EQ(0U, Offset(jt)) << "ICE: Failed to setup next instruction";
  }
  return Append(code, k, 0, 0);
}

CodeGen::Node CodeGen::WithinRange(Node target, size_t range) {
  // Just use |target| if it's already within range.
  if (Offset(target) <= range) {
    return target;
  }

  // Alternatively, look for an equivalent instruction within range.
  if (Offset(equivalent_.at(target)) <= range) {
    return equivalent_.at(target);
  }

  // Otherwise, fall back to emitting a jump instruction.
  Node jump = Append(BPF_JMP | BPF_JA, Offset(target), 0, 0);
  equivalent_.at(target) = jump;
  return jump;
}

CodeGen::Node CodeGen::Append(uint16_t code, uint32_t k, size_t jt, size_t jf) {
  if (BPF_CLASS(code) == BPF_JMP && BPF_OP(code) != BPF_JA) {
    CHECK_LE(jt, kBranchRange);
    CHECK_LE(jf, kBranchRange);
  } else {
    CHECK_EQ(0U, jt);
    CHECK_EQ(0U, jf);
  }

  CHECK_LT(program_.size(), static_cast<size_t>(BPF_MAXINSNS));
  CHECK_EQ(program_.size(), equivalent_.size());

  Node res = program_.size();
  program_.push_back(sock_filter{
      code, static_cast<uint8_t>(jt), static_cast<uint8_t>(jf), k});
  equivalent_.push_back(res);
  return res;
}

size_t CodeGen::Offset(Node target) const {
  CHECK_LT(target, program_.size()) << "Bogus offset target node";
  return (program_.size() - 1) - target;
}

}  // namespace sandbox
