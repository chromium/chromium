// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_CODEGEN_H__
#define SANDBOX_LINUX_BPF_DSL_CODEGEN_H__

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <tuple>
#include <vector>

#include "sandbox/sandbox_export.h"

struct sock_filter;

namespace sandbox {

// The code generator implements a basic assembler that can convert a
// graph of BPF instructions into a well-formed array of BPF
// instructions. Most notably, it ensures that jumps are always
// forward and don't exceed the limit of 255 instructions imposed by
// the instruction set.
//
// Callers would typically create a new CodeGen object and then use it
// to build a DAG of instruction nodes. They'll eventually call
// Compile() to convert this DAG to a Program.
//
//   CodeGen gen;
//   CodeGen::Node allow, branch, dag;
//
//   allow =
//     gen.MakeInstruction(BPF_RET+BPF_K,
//                         ErrorCode(ErrorCode::ERR_ALLOWED).err()));
//   branch =
//     gen.MakeInstruction(BPF_JMP+BPF_EQ+BPF_K, __NR_getpid,
//                         Trap(GetPidHandler, NULL), allow);
//   dag =
//     gen.MakeInstruction(BPF_LD+BPF_W+BPF_ABS,
//                         offsetof(struct arch_seccomp_data, nr), branch);
//
//   // Simplified code follows; in practice, it is important to avoid calling
//   // any C++ destructors after starting the sandbox.
//   CodeGen::Program program = gen.Compile(dag);
//   const struct sock_fprog prog = {
//     static_cast<unsigned short>(program.size()), &program[0] };
//   prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
//
class SANDBOX_EXPORT CodeGen {
 public:
  // A vector of BPF instructions that need to be installed as a filter
  // program in the kernel.
  typedef std::vector<struct sock_filter> Program;

  // Node represents a node within the instruction DAG being compiled.
  using Node = Program::size_type;

  // kNullNode represents the "null" node; i.e., the reserved node
  // value guaranteed to not equal any actual nodes.
  static const Node kNullNode = -1;

  CodeGen();

  CodeGen(const CodeGen&) = delete;
  CodeGen& operator=(const CodeGen&) = delete;

  ~CodeGen();

  // MakeInstruction creates a node representing the specified
  // instruction, or returns and existing equivalent node if one
  // exists. For details on the possible parameters refer to
  // https://www.kernel.org/doc/Documentation/networking/filter.txt.
  // TODO(mdempsky): Reconsider using default arguments here.
  Node MakeInstruction(uint16_t code,
                       uint32_t k,
                       Node jt = kNullNode,
                       Node jf = kNullNode);

  // Compile linearizes the instruction DAG rooted at |head| into a
  // program that can be executed by a BPF virtual machine.
  Program Compile(Node head);

 private:
  using MemoKey = std::tuple<uint16_t, uint32_t, Node, Node>;

  // AppendInstruction adds a new instruction, ensuring that |jt| and
  // |jf| are within range as necessary for |code|.
  Node AppendInstruction(uint16_t code, uint32_t k, Node jt, Node jf);

  // WithinRange returns a node equivalent to |next| that is at most
  // |range| instructions away from the (logical) beginning of the
  // program.
  Node WithinRange(Node next, size_t range);

  // Append appends a new instruction to the physical end (i.e.,
  // logical beginning) of |program_|.
  Node Append(uint16_t code, uint32_t k, size_t jt, size_t jf);

  // Offset returns how many instructions exist in |program_| after |target|.
  size_t Offset(Node target) const;

  // NOTE: program_ is the compiled program in *reverse*, so that
  // indices remain stable as we add instructions.
  Program program_;

  // equivalent_ stores the most recent semantically-equivalent node for each
  // instruction in program_. A node is defined as semantically-equivalent to N
  // if it has the same instruction code and constant as N and its successor
  // nodes (if any) are semantically-equivalent to N's successor nodes, or
  // if it's an unconditional jump to a node semantically-equivalent to N.
  std::vector<Node> equivalent_;

  std::map<MemoKey, Node> memos_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_CODEGEN_H__
