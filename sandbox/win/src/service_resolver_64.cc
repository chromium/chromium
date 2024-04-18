// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include <windows.h>

#include <ntstatus.h>
#include <stddef.h>
#include <winternl.h>

#include <memory>

#include "base/containers/heap_array.h"

namespace {
#if defined(_M_X64)
#pragma pack(push, 1)

const ULONG kMmovR10EcxMovEax = 0xB8D18B4C;
const USHORT kSyscall = 0x050F;
const BYTE kRetNp = 0xC3;
const ULONG64 kMov1 = 0x54894808244C8948;
const ULONG64 kMov2 = 0x4C182444894C1024;
const ULONG kMov3 = 0x20244C89;
const USHORT kTestByte = 0x04F6;
const BYTE kPtr = 0x25;
const BYTE kRet = 0xC3;
const USHORT kJne = 0x0375;

// Service code for 64 bit systems.
struct ServiceEntry {
  // This struct contains roughly the following code:
  // 00 mov     r10,rcx
  // 03 mov     eax,52h
  // 08 syscall
  // 0a ret
  // 0b xchg    ax,ax
  // 0e xchg    ax,ax

  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  BYTE pad;                   // = 66
  USHORT xchg_ax_ax1;         // = 66 90
  USHORT xchg_ax_ax2;         // = 66 90
};

// Service code for 64 bit Windows 8 and Windows 10 1507 (build 10240).
struct ServiceEntryW8 {
  // This struct contains the following code:
  // 00 48894c2408      mov     [rsp+8], rcx
  // 05 4889542410      mov     [rsp+10], rdx
  // 0a 4c89442418      mov     [rsp+18], r8
  // 0f 4c894c2420      mov     [rsp+20], r9
  // 14 4c8bd1          mov     r10,rcx
  // 17 b825000000      mov     eax,25h
  // 1c 0f05            syscall
  // 1e c3              ret
  // 1f 90              nop

  ULONG64 mov_1;              // = 48 89 4C 24 08 48 89 54
  ULONG64 mov_2;              // = 24 10 4C 89 44 24 18 4C
  ULONG mov_3;                // = 89 4C 24 20
  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  BYTE nop;                   // = 90
};

// Service code for 64 bit systems with int 2e fallback. Windows 10 1511+
struct ServiceEntryWithInt2E {
  // This struct contains roughly the following code:
  // 00 4c8bd1           mov     r10,rcx
  // 03 b855000000       mov     eax,52h
  // 08 f604250803fe7f01 test byte ptr SharedUserData!308, 1
  // 10 7503             jne [over syscall]
  // 12 0f05             syscall
  // 14 c3               ret
  // 15 cd2e             int 2e
  // 17 c3               ret

  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT test_byte;           // = F6 04
  BYTE ptr;                   // = 25
  ULONG user_shared_data_ptr;
  BYTE one;                   // = 01
  USHORT jne_over_syscall;    // = 75 03
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  USHORT int2e;               // = CD 2E
  BYTE ret2;                  // = C3
};

// We don't have an internal thunk for x64.
struct ServiceFullThunk {
  union {
    ServiceEntry original;
    ServiceEntryW8 original_w8;
    ServiceEntryWithInt2E original_int2e_fallback;
  };
};

#pragma pack(pop)

bool IsService(const void* source) {
  const ServiceEntry* service = reinterpret_cast<const ServiceEntry*>(source);

  return (kMmovR10EcxMovEax == service->mov_r10_rcx_mov_eax &&
          kSyscall == service->syscall && kRetNp == service->ret);
}

bool IsServiceW8(const void* source) {
  const ServiceEntryW8* service =
      reinterpret_cast<const ServiceEntryW8*>(source);

  return (kMmovR10EcxMovEax == service->mov_r10_rcx_mov_eax &&
          kMov1 == service->mov_1 && kMov2 == service->mov_2 &&
          kMov3 == service->mov_3);
}

bool IsServiceWithInt2E(const void* source) {
  const ServiceEntryWithInt2E* service =
      reinterpret_cast<const ServiceEntryWithInt2E*>(source);

  return (kMmovR10EcxMovEax == service->mov_r10_rcx_mov_eax &&
          kTestByte == service->test_byte && kPtr == service->ptr &&
          kJne == service->jne_over_syscall && kSyscall == service->syscall &&
          kRet == service->ret && kRet == service->ret2);
}

bool IsAnyService(const void* source) {
  return IsService(source) || IsServiceW8(source) || IsServiceWithInt2E(source);
}

#elif defined(_M_ARM64)
#pragma pack(push, 4)

const ULONG kSvc = 0xD4000001;
const ULONG kRetNp = 0xD65F03C0;
const ULONG kServiceIdMask = 0x001FFFE0;

struct ServiceEntry {
  ULONG svc;
  ULONG ret;
  ULONG64 unused;
};

struct ServiceFullThunk {
  ServiceEntry original;
};

#pragma pack(pop)

bool IsService(const void* source) {
  const ServiceEntry* service = reinterpret_cast<const ServiceEntry*>(source);

  return (kSvc == (service->svc & ~kServiceIdMask) && kRetNp == service->ret &&
          0 == service->unused);
}

bool IsAnyService(const void* source) {
  return IsService(source);
}

#else
#error "Unsupported Windows 64-bit Arch"
#endif

}  // namespace

namespace sandbox {

NTSTATUS ServiceResolverThunk::Setup(const void* target_module,
                                     const void* interceptor_module,
                                     const char* target_name,
                                     const char* interceptor_name,
                                     const void* interceptor_entry_point,
                                     void* thunk_storage,
                                     size_t storage_bytes,
                                     size_t* storage_used) {
  NTSTATUS ret =
      Init(target_module, interceptor_module, target_name, interceptor_name,
           interceptor_entry_point, thunk_storage, storage_bytes);
  if (!NT_SUCCESS(ret))
    return ret;

  size_t thunk_bytes = GetThunkSize();
  auto thunk_buffer = base::HeapArray<char>::Uninit(thunk_bytes);
  ServiceFullThunk* thunk =
      reinterpret_cast<ServiceFullThunk*>(thunk_buffer.data());

  if (!IsFunctionAService(&thunk->original))
    return STATUS_OBJECT_NAME_COLLISION;

  ret = PerformPatch(thunk, thunk_storage);

  if (storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

size_t ServiceResolverThunk::GetThunkSize() const {
  return sizeof(ServiceFullThunk);
}

NTSTATUS ServiceResolverThunk::CopyThunk(const void* target_module,
                                         const char* target_name,
                                         BYTE* thunk_storage,
                                         size_t storage_bytes,
                                         size_t* storage_used) {
  NTSTATUS ret = ResolveTarget(target_module, target_name, &target_);
  if (!NT_SUCCESS(ret))
    return ret;

  size_t thunk_bytes = GetThunkSize();
  if (storage_bytes < thunk_bytes)
    return STATUS_UNSUCCESSFUL;

  ServiceFullThunk* thunk = reinterpret_cast<ServiceFullThunk*>(thunk_storage);

  if (!IsFunctionAService(&thunk->original))
    return STATUS_OBJECT_NAME_COLLISION;

  if (storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

bool ServiceResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceFullThunk function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (!IsAnyService(&function_code))
    return false;

  // Save the verified code.
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  // Patch the original code.
  ServiceEntry local_service;
  if (!SetInternalThunk(&local_service, sizeof(local_service), nullptr,
                        interceptor_))
    return STATUS_UNSUCCESSFUL;

  // Copy the local thunk buffer to the child.
  SIZE_T actual;
  if (!::WriteProcessMemory(process_, remote_thunk, local_thunk,
                            sizeof(ServiceFullThunk), &actual))
    return STATUS_UNSUCCESSFUL;

  if (sizeof(ServiceFullThunk) != actual)
    return STATUS_UNSUCCESSFUL;

  // And now change the function to intercept, on the child.
  if (ntdll_base_) {
    // Running a unit test.
    if (!::WriteProcessMemory(process_, target_, &local_service,
                              sizeof(local_service), &actual))
      return STATUS_UNSUCCESSFUL;
  } else {
    if (!WriteProtectedChildMemory(process_, target_, &local_service,
                                   sizeof(local_service)))
      return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

bool ServiceResolverThunk::VerifyJumpTargetForTesting(void*) const {
  return true;
}

}  // namespace sandbox
