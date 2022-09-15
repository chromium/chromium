// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_I386_LINUX_UCONTEXT_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_I386_LINUX_UCONTEXT_H_

#include <stddef.h>
#include <stdint.h>

// This is mostly copied from breakpad (common/android/include/sys/ucontext.h),
// except we do use sigset_t for uc_sigmask instead of a custom type.

// In PNaCl toolchain, sigcontext is not defined. So here declare it.
typedef struct sigaltstack {
  void* ss_sp;
  int ss_flags;
  size_t ss_size;
} stack_t;

/* 80-bit floating-point register */
struct _libc_fpreg {
  unsigned short significand[4];
  unsigned short exponent;
};

/* Simple floating-point state, see FNSTENV instruction */
struct _libc_fpstate {
  unsigned long cw;
  unsigned long sw;
  unsigned long tag;
  unsigned long ipoff;
  unsigned long cssel;
  unsigned long dataoff;
  unsigned long datasel;
  struct _libc_fpreg _st[8];
  unsigned long status;
};

typedef uint32_t greg_t;

typedef struct {
  uint32_t gregs[19];
  struct _libc_fpstate* fpregs;
  uint32_t oldmask;
  uint32_t cr2;
} mcontext_t;

enum {
  REG_GS = 0,
  REG_FS,
  REG_ES,
  REG_DS,
  REG_EDI,
  REG_ESI,
  REG_EBP,
  REG_ESP,
  REG_EBX,
  REG_EDX,
  REG_ECX,
  REG_EAX,
  REG_TRAPNO,
  REG_ERR,
  REG_EIP,
  REG_CS,
  REG_EFL,
  REG_UESP,
  REG_SS,
};

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Android and PNaCl toolchain's sigset_t has only 32 bits, though Linux
  // ABI requires 64 bits.
  union {
    sigset_t uc_sigmask;
    uint32_t kernel_sigmask[2];
  };
  struct _libc_fpstate __fpregs_mem;
} ucontext_t;

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_I386_LINUX_UCONTEXT_H_
