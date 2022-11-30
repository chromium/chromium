// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header will be kept up to date so that we can compile system-call
// policies even when system headers are old.
// System call numbers are accessible through __NR_syscall_name.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SYSCALLS_H_

#include "build/build_config.h"

#if defined(__x86_64__)
#include "sandbox/linux/system_headers/x86_64_linux_syscalls.h"
#endif

#if defined(__i386__)
#include "sandbox/linux/system_headers/x86_32_linux_syscalls.h"
#endif

#if defined(__arm__) && defined(__ARM_EABI__)
#include "sandbox/linux/system_headers/arm_linux_syscalls.h"
#endif

#if (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
#include "sandbox/linux/system_headers/mips_linux_syscalls.h"
#endif

#if defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS)
#include "sandbox/linux/system_headers/mips64_linux_syscalls.h"
#endif

#if defined(__aarch64__)
#include "sandbox/linux/system_headers/arm64_linux_syscalls.h"
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SYSCALLS_H_

