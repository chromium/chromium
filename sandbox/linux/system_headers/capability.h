// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_CAPABILITY_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_CAPABILITY_H_

#include <stdint.h>

// The following macros are taken from linux/capability.h.
// We only support capability version 3, which was introduced in Linux 2.6.26.
#ifndef _LINUX_CAPABILITY_VERSION_3
#define _LINUX_CAPABILITY_VERSION_3 0x20080522
#endif
#ifndef _LINUX_CAPABILITY_U32S_3
#define _LINUX_CAPABILITY_U32S_3 2
#endif
#ifndef CAP_TO_INDEX
#define CAP_TO_INDEX(x) ((x) >> 5)  // 1 << 5 == bits in __u32
#endif
#ifndef CAP_TO_MASK
#define CAP_TO_MASK(x) (1 << ((x) & 31))  // mask for indexed __u32
#endif
#ifndef CAP_SYS_CHROOT
#define CAP_SYS_CHROOT 18
#endif
#ifndef CAP_SYS_ADMIN
#define CAP_SYS_ADMIN 21
#endif

struct cap_hdr {
  uint32_t version;
  int pid;
};

struct cap_data {
  uint32_t effective;
  uint32_t permitted;
  uint32_t inheritable;
};

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_CAPABILITY_H_
