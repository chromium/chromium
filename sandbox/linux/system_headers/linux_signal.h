// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_

#include <stdint.h>

#include "build/build_config.h"

// NOTE: On some toolchains, signal related ABI is incompatible with Linux's
// (not undefined, but defined different values and in different memory
// layouts). So, fill the gap here.
#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || \
    defined(__aarch64__)

#define LINUX_SIGHUP 1
#define LINUX_SIGINT 2
#define LINUX_SIGQUIT 3
#define LINUX_SIGABRT 6
#define LINUX_SIGBUS 7
#define LINUX_SIGUSR1 10
#define LINUX_SIGSEGV 11
#define LINUX_SIGUSR2 12
#define LINUX_SIGPIPE 13
#define LINUX_SIGTERM 15
#define LINUX_SIGCHLD 17
#define LINUX_SIGSYS 31

#define LINUX_SIG_BLOCK 0
#define LINUX_SIG_UNBLOCK 1

#define LINUX_SA_SIGINFO 4
#define LINUX_SA_NODEFER 0x40000000
#define LINUX_SA_RESTART 0x10000000

#define LINUX_SIG_DFL 0

#elif defined(__mips__)

#define LINUX_SIGHUP 1
#define LINUX_SIGINT 2
#define LINUX_SIGQUIT 3
#define LINUX_SIGABRT 6
#define LINUX_SIGBUS 10
#define LINUX_SIGSEGV 11
#define LINUX_SIGSYS 12
#define LINUX_SIGPIPE 13
#define LINUX_SIGTERM 15
#define LINUX_SIGUSR1 16
#define LINUX_SIGUSR2 17
#define LINUX_SIGCHLD 18

#define LINUX_SIG_BLOCK 1
#define LINUX_SIG_UNBLOCK 2

#define LINUX_SA_SIGINFO 0x00000008
#define LINUX_SA_NODEFER 0x40000000
#define LINUX_SA_RESTART 0x10000000

#define LINUX_SIG_DFL 0

#else
#error "Unsupported platform"
#endif

#include <signal.h>

static_assert(LINUX_SIGHUP == SIGHUP, "LINUX_SIGHUP == SIGHUP");
static_assert(LINUX_SIGINT == SIGINT, "LINUX_SIGINT == SIGINT");
static_assert(LINUX_SIGQUIT == SIGQUIT, "LINUX_SIGQUIT == SIGQUIT");
static_assert(LINUX_SIGABRT == SIGABRT, "LINUX_SIGABRT == SIGABRT");
static_assert(LINUX_SIGBUS == SIGBUS, "LINUX_SIGBUS == SIGBUS");
static_assert(LINUX_SIGUSR1 == SIGUSR1, "LINUX_SIGUSR1 == SIGUSR1");
static_assert(LINUX_SIGSEGV == SIGSEGV, "LINUX_SIGSEGV == SIGSEGV");
static_assert(LINUX_SIGUSR2 == SIGUSR2, "LINUX_SIGUSR2 == SIGUSR2");
static_assert(LINUX_SIGPIPE == SIGPIPE, "LINUX_SIGPIPE == SIGPIPE");
static_assert(LINUX_SIGTERM == SIGTERM, "LINUX_SIGTERM == SIGTERM");
static_assert(LINUX_SIGCHLD == SIGCHLD, "LINUX_SIGCHLD == SIGCHLD");
static_assert(LINUX_SIGSYS == SIGSYS, "LINUX_SIGSYS == SIGSYS");
static_assert(LINUX_SIG_BLOCK == SIG_BLOCK, "LINUX_SIG_BLOCK == SIG_BLOCK");
static_assert(LINUX_SIG_UNBLOCK == SIG_UNBLOCK,
              "LINUX_SIG_UNBLOCK == SIG_UNBLOCK");
static_assert(LINUX_SA_SIGINFO == SA_SIGINFO, "LINUX_SA_SIGINFO == SA_SIGINFO");
static_assert(LINUX_SA_NODEFER == SA_NODEFER, "LINUX_SA_NODEFER == SA_NODEFER");
static_assert(LINUX_SA_RESTART == SA_RESTART, "LINUX_SA_RESTART == SA_RESTART");
static_assert(LINUX_SIG_DFL == SIG_DFL, "LINUX_SIG_DFL == SIG_DFL");

typedef siginfo_t LinuxSigInfo;

// struct sigset_t is different size in PNaCl from the Linux's.
#if (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
#if !defined(_NSIG_WORDS)
#define _NSIG_WORDS 4
#endif
struct LinuxSigSet {
  unsigned long sig[_NSIG_WORDS];
};
#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS)
#if !defined(_NSIG_WORDS)
#define _NSIG_WORDS 2
#endif
struct LinuxSigSet {
  unsigned long sig[_NSIG_WORDS];
};
#else
typedef uint64_t LinuxSigSet;
#endif

// struct sigaction is different in PNaCl from the Linux's.
#if defined(__mips__)
struct LinuxSigAction {
  unsigned int sa_flags;
  void (*kernel_handler)(int);
  LinuxSigSet sa_mask;
};
#else
struct LinuxSigAction {
  void (*kernel_handler)(int);
  uint32_t sa_flags;
  void (*sa_restorer)(void);
  LinuxSigSet sa_mask;
};
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SIGNAL_H_
