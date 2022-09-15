// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_SECCOMP_MACROS_H_
#define SANDBOX_LINUX_BPF_DSL_SECCOMP_MACROS_H_

#include <sys/types.h>  // For __BIONIC__.
// Old Bionic versions do not have sys/user.h.  The if can be removed once we no
// longer need to support these old Bionic versions.
// All x86_64 builds use a new enough bionic to have sys/user.h.
#if !defined(__BIONIC__) || defined(__x86_64__)
#include <sys/user.h>
#if defined(__mips__)
// sys/user.h in eglibc misses size_t definition
#include <stddef.h>
#endif
#endif

#include "build/build_config.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"  // For AUDIT_ARCH_*

// Impose some reasonable maximum BPF program size. Realistically, the
// kernel probably has much lower limits. But by limiting to less than
// 30 bits, we can ease requirements on some of our data types.
#define SECCOMP_MAX_PROGRAM_SIZE (1<<30)

#if defined(__i386__)
#define SECCOMP_ARCH        AUDIT_ARCH_I386

#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.gregs[(_reg)])
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, REG_EAX)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, REG_EAX)
#define SECCOMP_IP(_ctx)        SECCOMP_REG(_ctx, REG_EIP)
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, REG_EBX)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, REG_ECX)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, REG_EDX)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, REG_ESI)
#define SECCOMP_PARM5(_ctx)     SECCOMP_REG(_ctx, REG_EDI)
#define SECCOMP_PARM6(_ctx)     SECCOMP_REG(_ctx, REG_EBP)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)

#if defined(__BIONIC__)
// Old Bionic versions don't have sys/user.h, so we just define regs_struct
// directly.  This can be removed once we no longer need to support these old
// Bionic versions.
struct regs_struct {
  long int ebx;
  long int ecx;
  long int edx;
  long int esi;
  long int edi;
  long int ebp;
  long int eax;
  long int xds;
  long int xes;
  long int xfs;
  long int xgs;
  long int orig_eax;
  long int eip;
  long int xcs;
  long int eflags;
  long int esp;
  long int xss;
};
#else
typedef user_regs_struct regs_struct;
#endif

#define SECCOMP_PT_RESULT(_regs)  (_regs).eax
#define SECCOMP_PT_SYSCALL(_regs) (_regs).orig_eax
#define SECCOMP_PT_IP(_regs)      (_regs).eip
#define SECCOMP_PT_PARM1(_regs)   (_regs).ebx
#define SECCOMP_PT_PARM2(_regs)   (_regs).ecx
#define SECCOMP_PT_PARM3(_regs)   (_regs).edx
#define SECCOMP_PT_PARM4(_regs)   (_regs).esi
#define SECCOMP_PT_PARM5(_regs)   (_regs).edi
#define SECCOMP_PT_PARM6(_regs)   (_regs).ebp

#elif defined(__x86_64__)
#define SECCOMP_ARCH        AUDIT_ARCH_X86_64

#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.gregs[(_reg)])
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, REG_RAX)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, REG_RAX)
#define SECCOMP_IP(_ctx)        SECCOMP_REG(_ctx, REG_RIP)
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, REG_RDI)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, REG_RSI)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, REG_RDX)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, REG_R10)
#define SECCOMP_PARM5(_ctx)     SECCOMP_REG(_ctx, REG_R8)
#define SECCOMP_PARM6(_ctx)     SECCOMP_REG(_ctx, REG_R9)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)

typedef user_regs_struct regs_struct;
#define SECCOMP_PT_RESULT(_regs)  (_regs).rax
#define SECCOMP_PT_SYSCALL(_regs) (_regs).orig_rax
#define SECCOMP_PT_IP(_regs)      (_regs).rip
#define SECCOMP_PT_PARM1(_regs)   (_regs).rdi
#define SECCOMP_PT_PARM2(_regs)   (_regs).rsi
#define SECCOMP_PT_PARM3(_regs)   (_regs).rdx
#define SECCOMP_PT_PARM4(_regs)   (_regs).r10
#define SECCOMP_PT_PARM5(_regs)   (_regs).r8
#define SECCOMP_PT_PARM6(_regs)   (_regs).r9

#elif defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__))
#define SECCOMP_ARCH AUDIT_ARCH_ARM

// ARM sigcontext_t is different from i386/x86_64.
// See </arch/arm/include/asm/sigcontext.h> in the Linux kernel.
#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.arm_##_reg)
// ARM EABI syscall convention.
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, r0)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, r7)
#define SECCOMP_IP(_ctx)        SECCOMP_REG(_ctx, pc)
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, r0)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, r1)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, r2)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, r3)
#define SECCOMP_PARM5(_ctx)     SECCOMP_REG(_ctx, r4)
#define SECCOMP_PARM6(_ctx)     SECCOMP_REG(_ctx, r5)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)

#if defined(__BIONIC__)
// Old Bionic versions don't have sys/user.h, so we just define regs_struct
// directly.  This can be removed once we no longer need to support these old
// Bionic versions.
struct regs_struct {
  unsigned long uregs[18];
};
#else
typedef user_regs regs_struct;
#endif

#define REG_cpsr    uregs[16]
#define REG_pc      uregs[15]
#define REG_lr      uregs[14]
#define REG_sp      uregs[13]
#define REG_ip      uregs[12]
#define REG_fp      uregs[11]
#define REG_r10     uregs[10]
#define REG_r9      uregs[9]
#define REG_r8      uregs[8]
#define REG_r7      uregs[7]
#define REG_r6      uregs[6]
#define REG_r5      uregs[5]
#define REG_r4      uregs[4]
#define REG_r3      uregs[3]
#define REG_r2      uregs[2]
#define REG_r1      uregs[1]
#define REG_r0      uregs[0]
#define REG_ORIG_r0 uregs[17]

#define SECCOMP_PT_RESULT(_regs)  (_regs).REG_r0
#define SECCOMP_PT_SYSCALL(_regs) (_regs).REG_r7
#define SECCOMP_PT_IP(_regs)      (_regs).REG_pc
#define SECCOMP_PT_PARM1(_regs)   (_regs).REG_r0
#define SECCOMP_PT_PARM2(_regs)   (_regs).REG_r1
#define SECCOMP_PT_PARM3(_regs)   (_regs).REG_r2
#define SECCOMP_PT_PARM4(_regs)   (_regs).REG_r3
#define SECCOMP_PT_PARM5(_regs)   (_regs).REG_r4
#define SECCOMP_PT_PARM6(_regs)   (_regs).REG_r5

#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS)
#define SECCOMP_ARCH        AUDIT_ARCH_MIPSEL
#define SYSCALL_EIGHT_ARGS
// MIPS sigcontext_t is different from i386/x86_64 and ARM.
// See </arch/mips/include/uapi/asm/sigcontext.h> in the Linux kernel.
#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.gregs[_reg])
// Based on MIPS o32 ABI syscall convention.
// On MIPS, when indirect syscall is being made (syscall(__NR_foo)),
// real identificator (__NR_foo) is not in v0, but in a0
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, 2)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, 2)
#define SECCOMP_IP(_ctx)        (_ctx)->uc_mcontext.pc
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, 4)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, 5)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, 6)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, 7)
// Only the first 4 arguments of syscall are in registers.
// The rest are on the stack.
#define SECCOMP_STACKPARM(_ctx, n)  (((long *)SECCOMP_REG(_ctx, 29))[(n)])
#define SECCOMP_PARM5(_ctx)         SECCOMP_STACKPARM(_ctx, 4)
#define SECCOMP_PARM6(_ctx)         SECCOMP_STACKPARM(_ctx, 5)
#define SECCOMP_PARM7(_ctx)         SECCOMP_STACKPARM(_ctx, 6)
#define SECCOMP_PARM8(_ctx)         SECCOMP_STACKPARM(_ctx, 7)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)

// On MIPS we don't have structures like user_regs or user_regs_struct in
// sys/user.h that we could use, so we just define regs_struct directly.
struct regs_struct {
  unsigned long long regs[32];
};

#define REG_a3 regs[7]
#define REG_a2 regs[6]
#define REG_a1 regs[5]
#define REG_a0 regs[4]
#define REG_v1 regs[3]
#define REG_v0 regs[2]

#define SECCOMP_PT_RESULT(_regs)  (_regs).REG_v0
#define SECCOMP_PT_SYSCALL(_regs) (_regs).REG_v0
#define SECCOMP_PT_PARM1(_regs)   (_regs).REG_a0
#define SECCOMP_PT_PARM2(_regs)   (_regs).REG_a1
#define SECCOMP_PT_PARM3(_regs)   (_regs).REG_a2
#define SECCOMP_PT_PARM4(_regs)   (_regs).REG_a3

#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS)
#define SECCOMP_ARCH        AUDIT_ARCH_MIPSEL64
#define SYSCALL_EIGHT_ARGS
// MIPS sigcontext_t is different from i386/x86_64 and ARM.
// See </arch/mips/include/uapi/asm/sigcontext.h> in the Linux kernel.
#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.gregs[_reg])
// Based on MIPS n64 ABI syscall convention.
// On MIPS, when an indirect syscall is being made (syscall(__NR_foo)),
// the real identifier (__NR_foo) is not in v0, but in a0.
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, 2)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, 2)
#define SECCOMP_IP(_ctx)        (_ctx)->uc_mcontext.pc
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, 4)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, 5)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, 6)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, 7)
#define SECCOMP_PARM5(_ctx)     SECCOMP_REG(_ctx, 8)
#define SECCOMP_PARM6(_ctx)     SECCOMP_REG(_ctx, 9)
#define SECCOMP_PARM7(_ctx)     SECCOMP_REG(_ctx, 10)
#define SECCOMP_PARM8(_ctx)     SECCOMP_REG(_ctx, 11)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)

// On MIPS we don't have structures like user_regs or user_regs_struct in
// sys/user.h that we could use, so we just define regs_struct directly.
struct regs_struct {
  unsigned long long regs[32];
};

#define REG_a7 regs[11]
#define REG_a6 regs[10]
#define REG_a5 regs[9]
#define REG_a4 regs[8]
#define REG_a3 regs[7]
#define REG_a2 regs[6]
#define REG_a1 regs[5]
#define REG_a0 regs[4]
#define REG_v1 regs[3]
#define REG_v0 regs[2]

#define SECCOMP_PT_RESULT(_regs)  (_regs).REG_v0
#define SECCOMP_PT_SYSCALL(_regs) (_regs).REG_v0
#define SECCOMP_PT_PARM1(_regs)   (_regs).REG_a0
#define SECCOMP_PT_PARM2(_regs)   (_regs).REG_a1
#define SECCOMP_PT_PARM3(_regs)   (_regs).REG_a2
#define SECCOMP_PT_PARM4(_regs)   (_regs).REG_a3
#define SECCOMP_PT_PARM5(_regs)   (_regs).REG_a4
#define SECCOMP_PT_PARM6(_regs)   (_regs).REG_a5
#define SECCOMP_PT_PARM7(_regs)   (_regs).REG_a6
#define SECCOMP_PT_PARM8(_regs)   (_regs).REG_a7

#elif defined(__aarch64__)
struct regs_struct {
  unsigned long long regs[31];
  unsigned long long sp;
  unsigned long long pc;
  unsigned long long pstate;
};

#define SECCOMP_ARCH AUDIT_ARCH_AARCH64

#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.regs[_reg])

#define SECCOMP_RESULT(_ctx) SECCOMP_REG(_ctx, 0)
#define SECCOMP_SYSCALL(_ctx) SECCOMP_REG(_ctx, 8)
#define SECCOMP_IP(_ctx) (_ctx)->uc_mcontext.pc
#define SECCOMP_PARM1(_ctx) SECCOMP_REG(_ctx, 0)
#define SECCOMP_PARM2(_ctx) SECCOMP_REG(_ctx, 1)
#define SECCOMP_PARM3(_ctx) SECCOMP_REG(_ctx, 2)
#define SECCOMP_PARM4(_ctx) SECCOMP_REG(_ctx, 3)
#define SECCOMP_PARM5(_ctx) SECCOMP_REG(_ctx, 4)
#define SECCOMP_PARM6(_ctx) SECCOMP_REG(_ctx, 5)

#define SECCOMP_NR_IDX (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX \
  (offsetof(struct arch_seccomp_data, instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX \
  (offsetof(struct arch_seccomp_data, instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) \
  (offsetof(struct arch_seccomp_data, args) + 8 * (nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) \
  (offsetof(struct arch_seccomp_data, args) + 8 * (nr) + 0)

#define SECCOMP_PT_RESULT(_regs) (_regs).regs[0]
#define SECCOMP_PT_SYSCALL(_regs) (_regs).regs[8]
#define SECCOMP_PT_IP(_regs) (_regs).pc
#define SECCOMP_PT_PARM1(_regs) (_regs).regs[0]
#define SECCOMP_PT_PARM2(_regs) (_regs).regs[1]
#define SECCOMP_PT_PARM3(_regs) (_regs).regs[2]
#define SECCOMP_PT_PARM4(_regs) (_regs).regs[3]
#define SECCOMP_PT_PARM5(_regs) (_regs).regs[4]
#define SECCOMP_PT_PARM6(_regs) (_regs).regs[5]
#else
#error Unsupported target platform

#endif

#endif  // SANDBOX_LINUX_BPF_DSL_SECCOMP_MACROS_H_
