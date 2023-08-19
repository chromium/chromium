// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/resolver.h"

#include <windows.h>

#include <ntstatus.h>
#include <stddef.h>

// For placement new. This file must not depend on the CRT at runtime, but
// placement operator new is inline.
#include <new>

namespace {

#if defined(_M_X64)

const USHORT kMovRax = 0xB848;
const USHORT kJmpRax = 0xe0ff;

#pragma pack(push, 1)
struct InternalThunk {
  // This struct contains roughly the following code:
  // 01 48b8f0debc9a78563412  mov   rax,123456789ABCDEF0h
  // ff e0                    jmp   rax
  //
  // The code modifies rax, but that's fine for x64 ABI.

  InternalThunk() {
    mov_rax = kMovRax;
    jmp_rax = kJmpRax;
    interceptor_function = 0;
  }
  USHORT mov_rax;  // = 48 B8
  ULONG_PTR interceptor_function;
  USHORT jmp_rax;  // = ff e0
};
#pragma pack(pop)

#elif defined(_M_ARM64)

const ULONG kLdrX16Pc4 = 0x58000050;
const ULONG kBrX16 = 0xD61F0200;

#pragma pack(push, 4)
struct InternalThunk {
  // This struct contains roughly the following code:
  // 00 58000050 ldr x16, pc+4
  // 04 D61F0200 br x16
  // 08 123456789ABCDEF0H

  InternalThunk() {
    ldr_x16_pc4 = kLdrX16Pc4;
    br_x16 = kBrX16;
    interceptor_function = 0;
  }
  ULONG ldr_x16_pc4;
  ULONG br_x16;
  ULONG_PTR interceptor_function;
};
#pragma pack(pop)
#else
#error "Unsupported Windows 64-bit Arch"
#endif

}  // namespace.

namespace sandbox {

size_t ResolverThunk::GetInternalThunkSize() const {
  return sizeof(InternalThunk);
}

bool ResolverThunk::SetInternalThunk(void* storage,
                                     size_t storage_bytes,
                                     const void* original_function,
                                     const void* interceptor) {
  if (storage_bytes < sizeof(InternalThunk))
    return false;

  InternalThunk* thunk = new (storage) InternalThunk;
  thunk->interceptor_function = reinterpret_cast<ULONG_PTR>(interceptor);

  return true;
}

NTSTATUS ResolverThunk::ResolveTarget(const void* module,
                                      const char* function_name,
                                      void** address) {
  // We don't support sidestep & co.
  return STATUS_NOT_IMPLEMENTED;
}

}  // namespace sandbox
