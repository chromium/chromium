// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_UCONTEXT_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_UCONTEXT_H_

#include <stddef.h>

// In PNaCl toolchain, sigcontext and stack_t is not defined. So here declare
// them.
struct sigcontext {
  unsigned long trap_no;
  unsigned long error_code;
  unsigned long oldmask;
  unsigned long arm_r0;
  unsigned long arm_r1;
  unsigned long arm_r2;
  unsigned long arm_r3;
  unsigned long arm_r4;
  unsigned long arm_r5;
  unsigned long arm_r6;
  unsigned long arm_r7;
  unsigned long arm_r8;
  unsigned long arm_r9;
  unsigned long arm_r10;
  unsigned long arm_fp;
  unsigned long arm_ip;
  unsigned long arm_sp;
  unsigned long arm_lr;
  unsigned long arm_pc;
  unsigned long arm_cpsr;
  unsigned long fault_address;
};

typedef struct sigaltstack {
  void* ss_sp;
  int ss_flags;
  size_t ss_size;
} stack_t;


// We also need greg_t for the sandbox, include it in this header as well.
typedef unsigned long greg_t;

// typedef unsigned long sigset_t;
typedef struct ucontext {
  unsigned long uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  struct sigcontext uc_mcontext;
  sigset_t uc_sigmask;
  /* Allow for uc_sigmask growth.  Glibc uses a 1024-bit sigset_t.  */
  int __not_used[32 - (sizeof(sigset_t) / sizeof(int))];
  /* Last for extensibility.  Eight byte aligned because some
     coprocessors require eight byte alignment.  */
  unsigned long uc_regspace[128] __attribute__((__aligned__(8)));
} ucontext_t;

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_UCONTEXT_H_
