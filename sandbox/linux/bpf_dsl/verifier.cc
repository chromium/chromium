// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/bpf_dsl/verifier.h"

#include <stdint.h>
#include <string.h>

#include "base/memory/raw_ref.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {

namespace {

struct State {
  State() = delete;

  State(const std::vector<struct sock_filter>& p,
        const struct arch_seccomp_data& d)
      : program(p), data(d), ip(0), accumulator(0), acc_is_valid(false) {}

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  const raw_ref<const std::vector<struct sock_filter>> program;
  const raw_ref<const struct arch_seccomp_data> data;
  unsigned int ip;
  uint32_t accumulator;
  bool acc_is_valid;
};

void Ld(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_SIZE(insn.code) != BPF_W || BPF_MODE(insn.code) != BPF_ABS ||
      insn.jt != 0 || insn.jf != 0) {
    *err = "Invalid BPF_LD instruction";
    return;
  }
  if (insn.k < sizeof(struct arch_seccomp_data) && (insn.k & 3) == 0) {
    // We only allow loading of properly aligned 32bit quantities.
    memcpy(&state->accumulator,
           reinterpret_cast<const char*>(&*state->data) + insn.k, 4);
  } else {
    *err = "Invalid operand in BPF_LD instruction";
    return;
  }
  state->acc_is_valid = true;
  return;
}

void Jmp(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_OP(insn.code) == BPF_JA) {
    if (state->ip + insn.k + 1 >= state->program->size() ||
        state->ip + insn.k + 1 <= state->ip) {
    compilation_failure:
      *err = "Invalid BPF_JMP instruction";
      return;
    }
    state->ip += insn.k;
  } else {
    if (BPF_SRC(insn.code) != BPF_K || !state->acc_is_valid ||
        state->ip + insn.jt + 1 >= state->program->size() ||
        state->ip + insn.jf + 1 >= state->program->size()) {
      goto compilation_failure;
    }
    switch (BPF_OP(insn.code)) {
      case BPF_JEQ:
        if (state->accumulator == insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JGT:
        if (state->accumulator > insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JGE:
        if (state->accumulator >= insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      case BPF_JSET:
        if (state->accumulator & insn.k) {
          state->ip += insn.jt;
        } else {
          state->ip += insn.jf;
        }
        break;
      default:
        goto compilation_failure;
    }
  }
}

uint32_t Ret(State*, const struct sock_filter& insn, const char** err) {
  if (BPF_SRC(insn.code) != BPF_K) {
    *err = "Invalid BPF_RET instruction";
    return 0;
  }
  return insn.k;
}

void Alu(State* state, const struct sock_filter& insn, const char** err) {
  if (BPF_OP(insn.code) == BPF_NEG) {
    state->accumulator = -state->accumulator;
    return;
  } else {
    if (BPF_SRC(insn.code) != BPF_K) {
      *err = "Unexpected source operand in arithmetic operation";
      return;
    }
    switch (BPF_OP(insn.code)) {
      case BPF_ADD:
        state->accumulator += insn.k;
        break;
      case BPF_SUB:
        state->accumulator -= insn.k;
        break;
      case BPF_MUL:
        state->accumulator *= insn.k;
        break;
      case BPF_DIV:
        if (!insn.k) {
          *err = "Illegal division by zero";
          break;
        }
        state->accumulator /= insn.k;
        break;
      case BPF_MOD:
        if (!insn.k) {
          *err = "Illegal division by zero";
          break;
        }
        state->accumulator %= insn.k;
        break;
      case BPF_OR:
        state->accumulator |= insn.k;
        break;
      case BPF_XOR:
        state->accumulator ^= insn.k;
        break;
      case BPF_AND:
        state->accumulator &= insn.k;
        break;
      case BPF_LSH:
        if (insn.k > 32) {
          *err = "Illegal shift operation";
          break;
        }
        state->accumulator <<= insn.k;
        break;
      case BPF_RSH:
        if (insn.k > 32) {
          *err = "Illegal shift operation";
          break;
        }
        state->accumulator >>= insn.k;
        break;
      default:
        *err = "Invalid operator in arithmetic operation";
        break;
    }
  }
}

}  // namespace

uint32_t Verifier::EvaluateBPF(const std::vector<struct sock_filter>& program,
                               const struct arch_seccomp_data& data,
                               const char** err) {
  *err = NULL;
  if (program.size() < 1 || program.size() >= SECCOMP_MAX_PROGRAM_SIZE) {
    *err = "Invalid program length";
    return 0;
  }
  for (State state(program, data); !*err; ++state.ip) {
    if (state.ip >= program.size()) {
      *err = "Invalid instruction pointer in BPF program";
      break;
    }
    const struct sock_filter& insn = program[state.ip];
    switch (BPF_CLASS(insn.code)) {
      case BPF_LD:
        Ld(&state, insn, err);
        break;
      case BPF_JMP:
        Jmp(&state, insn, err);
        break;
      case BPF_RET: {
        uint32_t r = Ret(&state, insn, err);
        switch (r & SECCOMP_RET_ACTION) {
          case SECCOMP_RET_ALLOW:
          case SECCOMP_RET_ERRNO:
          case SECCOMP_RET_KILL:
          case SECCOMP_RET_TRACE:
          case SECCOMP_RET_TRAP:
            break;
          case SECCOMP_RET_INVALID:  // Should never show up in BPF program
          default:
            *err = "Unexpected return code found in BPF program";
            return 0;
        }
        return r;
      }
      case BPF_ALU:
        Alu(&state, insn, err);
        break;
      default:
        *err = "Unexpected instruction in BPF program";
        break;
    }
  }
  return 0;
}

}  // namespace bpf_dsl
}  // namespace sandbox
