// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/resolver.h"

#include <windows.h>

#include <stddef.h>

// For placement new. This file must not depend on the CRT at runtime, but
// placement operator new is inline.
#include <new>

namespace {

#pragma pack(push, 1)
struct InternalThunk {
  // This struct contains roughly the following code:
  // sub esp, 8                             // Create working space
  // push edx                               // Save register
  // mov edx, [esp + 0xc]                   // Get return adddress
  // mov [esp + 8], edx                     // Store return address
  // mov dword ptr [esp + 0xc], 0x7c401200  // Store extra argument
  // mov dword ptr [esp + 4], 0x40010203    // Store address to jump to
  // pop edx                                // Restore register
  // ret                                    // Jump to interceptor
  //
  // This code only modifies esp and eip so it must work with to normal calling
  // convention. It is assembled as:
  //
  // 00 83ec08           sub     esp,8
  // 03 52               push    edx
  // 04 8b54240c         mov     edx,dword ptr [esp + 0Ch]
  // 08 89542408         mov     dword ptr [esp + 8], edx
  // 0c c744240c0012407c mov     dword ptr [esp + 0Ch], 7C401200h
  // 14 c744240403020140 mov     dword ptr [esp + 4], 40010203h
  // 1c 5a               pop     edx
  // 1d c3               ret
  InternalThunk() {
    opcodes_1 = 0x5208ec83;
    opcodes_2 = 0x0c24548b;
    opcodes_3 = 0x08245489;
    opcodes_4 = 0x0c2444c7;
    opcodes_5 = 0x042444c7;
    opcodes_6 = 0xc35a;
    extra_argument = 0;
    interceptor_function = 0;
  }
  ULONG opcodes_1;  // = 0x5208ec83
  ULONG opcodes_2;  // = 0x0c24548b
  ULONG opcodes_3;  // = 0x08245489
  ULONG opcodes_4;  // = 0x0c2444c7
  ULONG extra_argument;
  ULONG opcodes_5;  // = 0x042444c7
  ULONG interceptor_function;
  USHORT opcodes_6;  // = 0xc35a
};
#pragma pack(pop)

}  // namespace

namespace sandbox {

bool ResolverThunk::SetInternalThunk(void* storage,
                                     size_t storage_bytes,
                                     const void* original_function,
                                     const void* interceptor) {
  if (storage_bytes < sizeof(InternalThunk))
    return false;

  InternalThunk* thunk = new (storage) InternalThunk;

#pragma warning(push)
#pragma warning(disable : 4311)
  // These casts generate warnings because they are 32 bit specific.
  thunk->interceptor_function = reinterpret_cast<ULONG>(interceptor);
  thunk->extra_argument = reinterpret_cast<ULONG>(original_function);
#pragma warning(pop)

  return true;
}

size_t ResolverThunk::GetInternalThunkSize() const {
  return sizeof(InternalThunk);
}

NTSTATUS ResolverThunk::ResolveTarget(const void* module,
                                      const char* function_name,
                                      void** address) {
  const void** casted = const_cast<const void**>(address);
  return ResolverThunk::ResolveInterceptor(module, function_name, casted);
}

}  // namespace sandbox
