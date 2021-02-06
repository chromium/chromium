// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_

#include <stdint.h>

// The following structs and macros are taken from linux/filter.h,
// as some toolchain does not expose them.
struct sock_filter {
  uint16_t code;
  uint8_t jt;
  uint8_t jf;
  uint32_t k;
};

struct sock_fprog {
  uint16_t len;
  struct sock_filter *filter;
};

#ifndef BPF_CLASS
#define BPF_CLASS(code) ((code) & 0x07)
#endif

#ifndef BPF_LD
#define BPF_LD 0x00
#endif

#ifndef BPF_ALU
#define BPF_ALU 0x04
#endif

#ifndef BPF_JMP
#define BPF_JMP 0x05
#endif

#ifndef BPF_RET
#define BPF_RET 0x06
#endif

#ifndef BPF_SIZE
#define BPF_SIZE(code) ((code) & 0x18)
#endif

#ifndef BPF_W
#define BPF_W 0x00
#endif

#ifndef BPF_MODE
#define BPF_MODE(code) ((code) & 0xe0)
#endif

#ifndef BPF_ABS
#define BPF_ABS 0x20
#endif

#ifndef BPF_OP
#define BPF_OP(code) ((code) & 0xf0)
#endif

#ifndef BPF_ADD
#define BPF_ADD 0x00
#endif

#ifndef BPF_SUB
#define BPF_SUB 0x10
#endif

#ifndef BPF_MUL
#define BPF_MUL 0x20
#endif

#ifndef BPF_DIV
#define BPF_DIV 0x30
#endif

#ifndef BPF_OR
#define BPF_OR 0x40
#endif

#ifndef BPF_AND
#define BPF_AND 0x50
#endif

#ifndef BPF_LSH
#define BPF_LSH 0x60
#endif

#ifndef BPF_RSH
#define BPF_RSH 0x70
#endif

#ifndef BPF_NEG
#define BPF_NEG 0x80
#endif

#ifndef BPF_MOD
#define BPF_MOD 0x90
#endif

#ifndef BPF_XOR
#define BPF_XOR 0xA0
#endif

#ifndef BPF_JA
#define BPF_JA 0x00
#endif

#ifndef BPF_JEQ
#define BPF_JEQ 0x10
#endif

#ifndef BPF_JGT
#define BPF_JGT 0x20
#endif

#ifndef BPF_JGE
#define BPF_JGE 0x30
#endif

#ifndef BPF_JSET
#define BPF_JSET 0x40
#endif

#ifndef BPF_SRC
#define BPF_SRC(code) ((code) & 0x08)
#endif

#ifndef BPF_K
#define BPF_K 0x00
#endif

#ifndef BPF_MAXINSNS
#define BPF_MAXINSNS 4096
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_
