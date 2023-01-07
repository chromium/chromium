// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/dump_bpf.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {

namespace {

const char* AluOpToken(uint32_t code) {
  switch (BPF_OP(code)) {
    case BPF_ADD:
      return "+";
    case BPF_SUB:
      return "-";
    case BPF_MUL:
      return "*";
    case BPF_DIV:
      return "/";
    case BPF_MOD:
      return "%";
    case BPF_OR:
      return "|";
    case BPF_XOR:
      return "^";
    case BPF_AND:
      return "&";
    case BPF_LSH:
      return "<<";
    case BPF_RSH:
      return ">>";
    default:
      return "???";
  }
}

const char* JmpOpToken(uint32_t code) {
  switch (BPF_OP(code)) {
    case BPF_JSET:
      return "&";
    case BPF_JEQ:
      return "==";
    case BPF_JGE:
      return ">=";
    default:
      return "???";
  }
}

const char* DataOffsetName(size_t off) {
  switch (off) {
    case SECCOMP_NR_IDX:
      return "System call number";
    case SECCOMP_ARCH_IDX:
      return "Architecture";
    case SECCOMP_IP_LSB_IDX:
      return "Instruction pointer (LSB)";
    case SECCOMP_IP_MSB_IDX:
      return "Instruction pointer (MSB)";
    default:
      return "???";
  }
}

void AppendInstruction(std::string* dst, size_t pc, const sock_filter& insn) {
  base::StringAppendF(dst, "%3zu) ", pc);
  switch (BPF_CLASS(insn.code)) {
    case BPF_LD:
      if (insn.code == BPF_LD + BPF_W + BPF_ABS) {
        base::StringAppendF(dst, "LOAD %" PRIu32 "  // ", insn.k);
        size_t maybe_argno =
            (insn.k - offsetof(struct arch_seccomp_data, args)) /
            sizeof(uint64_t);
        if (maybe_argno < 6 && insn.k == SECCOMP_ARG_LSB_IDX(maybe_argno)) {
          base::StringAppendF(dst, "Argument %zu (LSB)\n", maybe_argno);
        } else if (maybe_argno < 6 &&
                   insn.k == SECCOMP_ARG_MSB_IDX(maybe_argno)) {
          base::StringAppendF(dst, "Argument %zu (MSB)\n", maybe_argno);
        } else {
          base::StringAppendF(dst, "%s\n", DataOffsetName(insn.k));
        }
      } else {
        base::StringAppendF(dst, "Load ???\n");
      }
      break;
    case BPF_JMP:
      if (BPF_OP(insn.code) == BPF_JA) {
        base::StringAppendF(dst, "JMP %zu\n", pc + insn.k + 1);
      } else {
        base::StringAppendF(
            dst, "if A %s 0x%" PRIx32 "; then JMP %zu else JMP %zu\n",
            JmpOpToken(insn.code), insn.k, pc + insn.jt + 1, pc + insn.jf + 1);
      }
      break;
    case BPF_RET:
      base::StringAppendF(dst, "RET 0x%" PRIx32 "  // ", insn.k);
      if ((insn.k & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP) {
        base::StringAppendF(dst, "Trap #%" PRIu32 "\n",
                            insn.k & SECCOMP_RET_DATA);
      } else if ((insn.k & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
        base::StringAppendF(dst, "errno = %" PRIu32 "\n",
                            insn.k & SECCOMP_RET_DATA);
      } else if ((insn.k & SECCOMP_RET_ACTION) == SECCOMP_RET_TRACE) {
        base::StringAppendF(dst, "Trace #%" PRIu32 "\n",
                            insn.k & SECCOMP_RET_DATA);
      } else if (insn.k == SECCOMP_RET_USER_NOTIF) {
        base::StringAppendF(dst, "UserNotif\n");
      } else if (insn.k == SECCOMP_RET_ALLOW) {
        base::StringAppendF(dst, "Allowed\n");
      } else if (insn.k == SECCOMP_RET_KILL) {
        base::StringAppendF(dst, "Kill\n");
      } else {
        base::StringAppendF(dst, "???\n");
      }
      break;
    case BPF_ALU:
      if (BPF_OP(insn.code) == BPF_NEG) {
        base::StringAppendF(dst, "A := -A\n");
      } else {
        base::StringAppendF(dst, "A := A %s 0x%" PRIx32 "\n",
                            AluOpToken(insn.code), insn.k);
      }
      break;
    default:
      base::StringAppendF(dst, "???\n");
      break;
  }
}

}  // namespace

void DumpBPF::PrintProgram(const CodeGen::Program& program) {
  fputs(StringPrintProgram(program).c_str(), stderr);
}

std::string DumpBPF::StringPrintProgram(const CodeGen::Program& program) {
  std::string res;
  for (size_t i = 0; i < program.size(); i++) {
    AppendInstruction(&res, i + 1, program[i]);
  }
  return res;
}

}  // namespace bpf_dsl
}  // namespace sandbox
