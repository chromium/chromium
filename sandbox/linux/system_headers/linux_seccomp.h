// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SECCOMP_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SECCOMP_H_

#include <stdint.h>
#include <sys/ioctl.h>

#include "build/build_config.h"

// The Seccomp2 kernel ABI is not part of older versions of glibc.
// As we can't break compilation with these versions of the library,
// we explicitly define all missing symbols.
// If we ever decide that we can now rely on system headers, the following
// include files should be enabled:
// #include <linux/audit.h>
// #include <linux/seccomp.h>

// For audit.h
#ifndef EM_ARM
#define EM_ARM    40
#endif

#ifndef EM_386
#define EM_386    3
#endif

#ifndef EM_X86_64
#define EM_X86_64 62
#endif

#ifndef EM_MIPS
#define EM_MIPS   8
#endif

#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif

#ifndef __AUDIT_ARCH_64BIT
#define __AUDIT_ARCH_64BIT 0x80000000
#endif

#ifndef __AUDIT_ARCH_LE
#define __AUDIT_ARCH_LE    0x40000000
#endif

#ifndef AUDIT_ARCH_ARM
#define AUDIT_ARCH_ARM    (EM_ARM|__AUDIT_ARCH_LE)
#endif

#ifndef AUDIT_ARCH_I386
#define AUDIT_ARCH_I386   (EM_386|__AUDIT_ARCH_LE)
#endif

#ifndef AUDIT_ARCH_X86_64
#define AUDIT_ARCH_X86_64 (EM_X86_64|__AUDIT_ARCH_64BIT|__AUDIT_ARCH_LE)
#endif

#ifndef AUDIT_ARCH_MIPSEL
#define AUDIT_ARCH_MIPSEL (EM_MIPS|__AUDIT_ARCH_LE)
#endif

#ifndef AUDIT_ARCH_MIPSEL64
#define AUDIT_ARCH_MIPSEL64 (EM_MIPS|__AUDIT_ARCH_64BIT|__AUDIT_ARCH_LE)
#endif

#ifndef AUDIT_ARCH_AARCH64
#define AUDIT_ARCH_AARCH64 (EM_AARCH64 | __AUDIT_ARCH_64BIT | __AUDIT_ARCH_LE)
#endif

// For prctl.h
#ifndef PR_SET_SECCOMP
#define PR_SET_SECCOMP               22
#define PR_GET_SECCOMP               21
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS          38
#define PR_GET_NO_NEW_PRIVS          39
#endif

#ifndef IPC_64
#define IPC_64                   0x0100
#endif

#ifndef PR_SET_SPECULATION_CTRL
#define PR_SET_SPECULATION_CTRL 53
#define PR_GET_SPECULATION_CTRL 52
#endif

#ifndef PR_SPEC_INDIRECT_BRANCH
#define PR_SPEC_INDIRECT_BRANCH 1
#endif

#ifndef PR_SPEC_PRCTL
#define PR_SPEC_PRCTL (1UL << 0)
#endif

#ifndef PR_SPEC_FORCE_DISABLE
#define PR_SPEC_FORCE_DISABLE (1UL << 3)
#endif

// In order to build will older tool chains, we currently have to avoid
// including <linux/seccomp.h>. Until that can be fixed (if ever). Rely on
// our own definitions of the seccomp kernel ABI.
#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_DISABLED         0
#define SECCOMP_MODE_STRICT           1
#define SECCOMP_MODE_FILTER           2  // User user-supplied filter
#endif

#ifndef SECCOMP_SET_MODE_STRICT
#define SECCOMP_SET_MODE_STRICT 0
#endif
#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER 1
#endif
#ifndef SECCOMP_GET_NOTIF_SIZES
#define SECCOMP_GET_NOTIF_SIZES 3
#endif

#ifndef SECCOMP_FILTER_FLAG_TSYNC
#define SECCOMP_FILTER_FLAG_TSYNC 1
#endif

#ifndef SECCOMP_FILTER_FLAG_SPEC_ALLOW
#define SECCOMP_FILTER_FLAG_SPEC_ALLOW (1UL << 2)
#endif

#ifndef SECCOMP_FILTER_FLAG_NEW_LISTENER
#define SECCOMP_FILTER_FLAG_NEW_LISTENER (1UL << 3)
#endif

#ifndef SECCOMP_FILTER_FLAG_TSYNC_ESRCH
#define SECCOMP_FILTER_FLAG_TSYNC_ESRCH (1UL << 4)
#endif

#ifndef SECCOMP_ADDFD_FLAG_SETFD
#define SECCOMP_ADDFD_FLAG_SETFD (1UL << 0)
#endif

// In the future, if we add fields to these structs and then access them, they
// might be out-of-bounds on an older kernel. So before adding to these structs,
// make sure to annotate them with a comment that it may be unsafe to access
// those fields on older kernels.
struct arch_seccomp_data {
  int nr;
  uint32_t arch;
  uint64_t instruction_pointer;
  uint64_t args[6];
};

struct seccomp_notif_sizes {
  uint16_t seccomp_notif;
  uint16_t seccomp_notif_resp;
  uint16_t seccomp_data;
};

struct seccomp_notif {
  uint64_t id;
  uint32_t pid;
  uint32_t flags;
  struct arch_seccomp_data data;
};

struct seccomp_notif_resp {
  uint64_t id;
  int64_t val;
  int32_t error;
  uint32_t flags;
};

struct seccomp_notif_addfd {
  uint64_t id;
  uint32_t flags;
  uint32_t srcfd;
  uint32_t newfd;
  uint32_t newfd_flags;
};

#define SECCOMP_IOC_MAGIC '!'
#define SECCOMP_IO(nr) _IO(SECCOMP_IOC_MAGIC, nr)
#define SECCOMP_IOR(nr, type) _IOR(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOW(nr, type) _IOW(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOWR(nr, type) _IOWR(SECCOMP_IOC_MAGIC, nr, type)

// Flags for seccomp notification fd ioctl.
#define SECCOMP_IOCTL_NOTIF_RECV SECCOMP_IOWR(0, struct seccomp_notif)
#define SECCOMP_IOCTL_NOTIF_SEND SECCOMP_IOWR(1, struct seccomp_notif_resp)
// Note: SECCOMP_IOCTL_NOTIF_ID_VALID is now defined with SECCOMP_IOW, but
// kernels are expected to support the (now incorrect) ioctl number for the
// foreseeable future.
#define SECCOMP_IOCTL_NOTIF_ID_VALID SECCOMP_IOR(2, uint64_t)
// On success, the return value is the remote process's added fd number
#define SECCOMP_IOCTL_NOTIF_ADDFD SECCOMP_IOW(3, struct seccomp_notif_addfd)

#ifndef SECCOMP_RET_KILL
// Return values supported for BPF filter programs. Please note that the
// "illegal" SECCOMP_RET_INVALID is not supported by the kernel, should only
// ever be used internally, and would result in the kernel killing our process.
#define SECCOMP_RET_KILL 0x00000000U        // Kill the task immediately
#define SECCOMP_RET_INVALID 0x00010000U     // Illegal return value
#define SECCOMP_RET_TRAP 0x00030000U        // Disallow and force a SIGSYS
#define SECCOMP_RET_ERRNO 0x00050000U       // Returns an errno
#define SECCOMP_RET_USER_NOTIF 0x7fc00000U  // Notifies userspace
#define SECCOMP_RET_TRACE 0x7ff00000U       // Pass to a tracer or disallow
#define SECCOMP_RET_ALLOW 0x7fff0000U       // Allow
#define SECCOMP_RET_ACTION 0xffff0000U      // Masks for the return value
#define SECCOMP_RET_DATA 0x0000ffffU        //   sections
#else
#define SECCOMP_RET_INVALID 0x00010000U  // Illegal return value
#endif

#ifndef SYS_SECCOMP
#define SYS_SECCOMP                   1
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_SECCOMP_H_
