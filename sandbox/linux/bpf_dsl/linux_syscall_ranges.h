// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_LINUX_SYSCALL_RANGES_H_
#define SANDBOX_LINUX_BPF_DSL_LINUX_SYSCALL_RANGES_H_

#include "build/build_config.h"

#if defined(__x86_64__)

#define MIN_SYSCALL         0u
#define MAX_PUBLIC_SYSCALL  1024u
#define MAX_SYSCALL         MAX_PUBLIC_SYSCALL

#elif defined(__i386__)

#define MIN_SYSCALL         0u
#define MAX_PUBLIC_SYSCALL  1024u
#define MAX_SYSCALL         MAX_PUBLIC_SYSCALL

#elif defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__))

// ARM EABI includes "ARM private" system calls starting at |__ARM_NR_BASE|,
// and a "ghost syscall private to the kernel", cmpxchg,
// at |__ARM_NR_BASE+0x00fff0|.
// See </arch/arm/include/asm/unistd.h> in the Linux kernel.

// __NR_SYSCALL_BASE is 0 in thumb and ARM EABI.
#define MIN_SYSCALL         0u
#define MAX_PUBLIC_SYSCALL  (MIN_SYSCALL + 1024u)
// __ARM_NR_BASE is __NR_SYSCALL_BASE + 0xf0000u
#define MIN_PRIVATE_SYSCALL 0xf0000u
#define MAX_PRIVATE_SYSCALL (MIN_PRIVATE_SYSCALL + 16u)
#define MIN_GHOST_SYSCALL   (MIN_PRIVATE_SYSCALL + 0xfff0u)
#define MAX_SYSCALL         (MIN_GHOST_SYSCALL + 4u)

#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS)

#include <asm/unistd.h>  // for __NR_O32_Linux and __NR_Linux_syscalls
#define MIN_SYSCALL         __NR_O32_Linux
#define MAX_PUBLIC_SYSCALL  (MIN_SYSCALL + __NR_Linux_syscalls)
#define MAX_SYSCALL         MAX_PUBLIC_SYSCALL

#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS)

#include <asm/unistd.h>  // for __NR_64_Linux and __NR_64_Linux_syscalls
#define MIN_SYSCALL         __NR_64_Linux
#define MAX_PUBLIC_SYSCALL  (MIN_SYSCALL + __NR_64_Linux_syscalls)
#define MAX_SYSCALL         MAX_PUBLIC_SYSCALL

#elif defined(__aarch64__)

#include <asm-generic/unistd.h>
#define MIN_SYSCALL 0u
#define MAX_PUBLIC_SYSCALL __NR_syscalls
#define MAX_SYSCALL MAX_PUBLIC_SYSCALL

#else
#error "Unsupported architecture"
#endif

#endif  // SANDBOX_LINUX_BPF_DSL_LINUX_SYSCALL_RANGES_H_
