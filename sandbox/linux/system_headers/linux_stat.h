// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_STAT_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_STAT_H_

#include <stdint.h>

#include "build/build_config.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if defined(ARCH_CPU_MIPS_FAMILY)
struct kernel_stat64 {
  unsigned st_dev;
  unsigned __pad0[3];
  unsigned long long st_ino;
  unsigned st_mode;
  unsigned st_nlink;
  unsigned st_uid;
  unsigned st_gid;
  unsigned st_rdev;
  unsigned __pad1[3];
  long long st_size;
  unsigned st_atime_;
  unsigned st_atime_nsec_;
  unsigned st_mtime_;
  unsigned st_mtime_nsec_;
  unsigned st_ctime_;
  unsigned st_ctime_nsec_;
  unsigned st_blksize;
  unsigned __pad2;
  unsigned long long st_blocks;
};
#else
struct kernel_stat64 {
  unsigned long long st_dev;
  unsigned char __pad0[4];
  unsigned __st_ino;
  unsigned st_mode;
  unsigned st_nlink;
  unsigned st_uid;
  unsigned st_gid;
  unsigned long long st_rdev;
  unsigned char __pad3[4];
  long long st_size;
  unsigned st_blksize;
  unsigned long long st_blocks;
  unsigned st_atime_;
  unsigned st_atime_nsec_;
  unsigned st_mtime_;
  unsigned st_mtime_nsec_;
  unsigned st_ctime_;
  unsigned st_ctime_nsec_;
  unsigned long long st_ino;
};
#endif

#if defined(__i386__) || defined(__ARM_ARCH_3__) || defined(__ARM_EABI__)
struct kernel_stat {
  /* The kernel headers suggest that st_dev and st_rdev should be 32bit
   * quantities encoding 12bit major and 20bit minor numbers in an interleaved
   * format. In reality, we do not see useful data in the top bits. So,
   * we'll leave the padding in here, until we find a better solution.
   */
  unsigned short st_dev;
  short pad1;
  unsigned st_ino;
  unsigned short st_mode;
  unsigned short st_nlink;
  unsigned short st_uid;
  unsigned short st_gid;
  unsigned short st_rdev;
  short pad2;
  unsigned st_size;
  unsigned st_blksize;
  unsigned st_blocks;
  unsigned st_atime_;
  unsigned st_atime_nsec_;
  unsigned st_mtime_;
  unsigned st_mtime_nsec_;
  unsigned st_ctime_;
  unsigned st_ctime_nsec_;
  unsigned __unused4;
  unsigned __unused5;
};
#elif defined(__x86_64__)
struct kernel_stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_nlink;
  unsigned st_mode;
  unsigned st_uid;
  unsigned st_gid;
  unsigned __pad0;
  uint64_t st_rdev;
  int64_t st_size;
  int64_t st_blksize;
  int64_t st_blocks;
  uint64_t st_atime_;
  uint64_t st_atime_nsec_;
  uint64_t st_mtime_;
  uint64_t st_mtime_nsec_;
  uint64_t st_ctime_;
  uint64_t st_ctime_nsec_;
  int64_t __unused4[3];
};
#elif (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_64_BITS))
struct kernel_stat {
  unsigned st_dev;
  unsigned __pad0[3];
  unsigned long st_ino;
  unsigned st_mode;
  unsigned st_nlink;
  unsigned st_uid;
  unsigned st_gid;
  unsigned st_rdev;
  unsigned __pad1[3];
  long st_size;
  unsigned st_atime_;
  unsigned st_atime_nsec_;
  unsigned st_mtime_;
  unsigned st_mtime_nsec_;
  unsigned st_ctime_;
  unsigned st_ctime_nsec_;
  unsigned st_blksize;
  unsigned __pad2;
  unsigned long st_blocks;
};
#elif (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
struct kernel_stat {
  unsigned st_dev;
  int st_pad1[3];
  unsigned st_ino;
  unsigned st_mode;
  unsigned st_nlink;
  unsigned st_uid;
  unsigned st_gid;
  unsigned st_rdev;
  int st_pad2[2];
  long st_size;
  int st_pad3;
  long st_atime_;
  long st_atime_nsec_;
  long st_mtime_;
  long st_mtime_nsec_;
  long st_ctime_;
  long st_ctime_nsec_;
  int st_blksize;
  int st_blocks;
  int st_pad4[14];
};
#elif defined(__aarch64__)
struct kernel_stat {
  unsigned long st_dev;
  unsigned long st_ino;
  unsigned int st_mode;
  unsigned int st_nlink;
  unsigned int st_uid;
  unsigned int st_gid;
  unsigned long st_rdev;
  unsigned long __pad1;
  long st_size;
  int st_blksize;
  int __pad2;
  long st_blocks;
  long st_atime_;
  unsigned long st_atime_nsec_;
  long st_mtime_;
  unsigned long st_mtime_nsec_;
  long st_ctime_;
  unsigned long st_ctime_nsec_;
  unsigned int __unused4;
  unsigned int __unused5;
};
#endif

#if !defined(AT_EMPTY_PATH)
#define AT_EMPTY_PATH 0x1000
#endif

#if !defined(STATX_BASIC_STATS)
#define STATX_BASIC_STATS 0x000007ffU
#endif

// On 32-bit systems, we default to the 64-bit stat struct like libc
// implementations do. Otherwise we default to the normal stat struct which is
// already 64-bit.
// These defines make it easy to call the right syscall to fill out a 64-bit
// stat struct, which is the default in libc implementations but requires
// different syscall names on 32 and 64-bit platforms.
#if defined(__NR_fstatat64)

namespace sandbox {
using default_stat_struct = struct kernel_stat64;
}  // namespace sandbox

#define __NR_fstatat_default __NR_fstatat64
#define __NR_fstat_default __NR_fstat64

#elif defined(__NR_newfstatat)

namespace sandbox {
using default_stat_struct = struct kernel_stat;
}  // namespace sandbox

#define __NR_fstatat_default __NR_newfstatat
#define __NR_fstat_default __NR_fstat

#else
#error "one of fstatat64 and newfstatat must be defined"
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_STAT_H_
