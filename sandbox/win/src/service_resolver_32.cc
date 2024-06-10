// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include <windows.h>

#include <ntstatus.h>
#include <stddef.h>
#include <winternl.h>

#include "base/containers/heap_array.h"

namespace {
#pragma pack(push, 1)

const BYTE kMovEax = 0xB8;
const BYTE kMovEdx = 0xBA;
const USHORT kMovEdxEsp = 0xD48B;
const USHORT kCallEdx = 0xD2FF;
const BYTE kCallEip = 0xE8;
const BYTE kRet = 0xC2;
const BYTE kRet2 = 0xC3;
const USHORT kJmpEdx = 0xE2FF;
const BYTE kJmp32 = 0xE9;
const USHORT kSysenter = 0x340F;

// Service code for 32 bit Windows. Introduced in Windows 8.
struct ServiceEntry32 {
  // This struct contains the following code:
  // 00 b825000000      mov     eax,25h
  // 05 e803000000      call    eip+3
  // 0a c22c00          ret     2Ch
  // 0d 8bd4            mov     edx,esp
  // 0f 0f34            sysenter
  // 11 c3              ret
  // 12 8bff            mov     edi,edi
  BYTE mov_eax;         // = B8
  ULONG service_id;
  BYTE call_eip;        // = E8
  ULONG call_offset;
  BYTE ret_p;           // = C2
  USHORT num_params;
  USHORT mov_edx_esp;   // = BD D4
  USHORT sysenter;      // = 0F 34
  BYTE ret;             // = C3
  USHORT nop;
};

// Service code for a 32 bit process under Wow64. Introduced in Windows 10.
// Also used for the patching process.
struct ServiceEntryWow64 {
  // 00 b828000000      mov     eax, 28h
  // 05 bab0d54877      mov     edx, 7748D5B0h
  // 09 ffd2            call    edx
  // 0c c22800          ret     28h
  BYTE mov_eax;         // = B8
  ULONG service_id;
  BYTE mov_edx;         // = BA
  ULONG mov_edx_param;
  USHORT call_edx;      // = FF D2
  BYTE ret;             // = C2
  USHORT num_params;
  BYTE nop;
};

// Make sure that relaxed patching works as expected.
const size_t kMinServiceSize = offsetof(ServiceEntryWow64, ret);
// Maximum size of the entry, was the size of the Windows Vista WoW64 entry.
// Keep this fixed for compatibility reasons.
const size_t kMaxServiceSize = 24;
static_assert(sizeof(ServiceEntry32) >= kMinServiceSize,
              "wrong minimum service length");
static_assert(sizeof(ServiceEntry32) < kMaxServiceSize,
              "wrong maximum service length");
static_assert(sizeof(ServiceEntryWow64) >= kMinServiceSize,
              "wrong minimum service length");
static_assert(sizeof(ServiceEntryWow64) < kMaxServiceSize,
              "wrong maximum service length");

struct ServiceFullThunk {
  union {
    ServiceEntryWow64 original;
    // Pad the entry to the maximum size.
    char dummy[kMaxServiceSize];
  };
  int internal_thunk;  // Dummy member to the beginning of the internal thunk.
};

#pragma pack(pop)

bool IsWow64Process() {
  // We don't need to use IsWow64Process2 as this returns the expected result
  // when running in the ARM64 x86 emulator.
  BOOL is_wow64 = FALSE;
  return ::IsWow64Process(::GetCurrentProcess(), &is_wow64) && is_wow64;
}

bool IsFunctionAService32(HANDLE process, void* target, void* local_thunk) {
  ServiceEntry32 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process, target, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kCallEip != function_code.call_eip ||
      function_code.call_offset != 3 || kRet != function_code.ret_p ||
      kMovEdxEsp != function_code.mov_edx_esp ||
      kSysenter != function_code.sysenter || kRet2 != function_code.ret) {
    return false;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

bool IsFunctionAServiceWow64(HANDLE process, void* target, void* local_thunk) {
  ServiceEntryWow64 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process, target, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kMovEdx != function_code.mov_edx ||
      kCallEdx != function_code.call_edx || kRet != function_code.ret) {
    return false;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));
  return true;
}

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

  relative_jump_ = 0;
  size_t thunk_bytes = GetThunkSize();
  base::HeapArray<char> thunk_buffer =
      base::HeapArray<char>::Uninit(thunk_bytes);
  ServiceFullThunk* thunk =
      reinterpret_cast<ServiceFullThunk*>(thunk_buffer.data());

  if (!IsFunctionAService(&thunk->original) &&
      (!relaxed_ || !SaveOriginalFunction(&thunk->original, thunk_storage))) {
    return STATUS_OBJECT_NAME_COLLISION;
  }

  ret = PerformPatch(thunk, thunk_storage);

  if (storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

size_t ServiceResolverThunk::GetThunkSize() const {
  return offsetof(ServiceFullThunk, internal_thunk) + GetInternalThunkSize();
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

  if (!IsFunctionAService(&thunk->original) &&
      (!relaxed_ || !SaveOriginalFunction(&thunk->original, thunk_storage))) {
    return STATUS_OBJECT_NAME_COLLISION;
  }

  if (storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

bool ServiceResolverThunk::IsFunctionAService(void* local_thunk) const {
  static bool is_wow64 = IsWow64Process();
  return is_wow64 ? IsFunctionAServiceWow64(process_, target_, local_thunk)
                  : IsFunctionAService32(process_, target_, local_thunk);
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  ServiceEntryWow64 intercepted_code;
  size_t bytes_to_write = sizeof(intercepted_code);
  ServiceFullThunk* full_local_thunk =
      reinterpret_cast<ServiceFullThunk*>(local_thunk);
  ServiceFullThunk* full_remote_thunk =
      reinterpret_cast<ServiceFullThunk*>(remote_thunk);

  // patch the original code
  memcpy(&intercepted_code, &full_local_thunk->original,
         sizeof(intercepted_code));
  intercepted_code.mov_eax = kMovEax;
  intercepted_code.service_id = full_local_thunk->original.service_id;
  intercepted_code.mov_edx = kMovEdx;
  intercepted_code.mov_edx_param =
      reinterpret_cast<ULONG>(&full_remote_thunk->internal_thunk);
  intercepted_code.call_edx = kJmpEdx;
  bytes_to_write = kMinServiceSize;

  if (relative_jump_) {
    intercepted_code.mov_eax = kJmp32;
    intercepted_code.service_id = relative_jump_;
    bytes_to_write = offsetof(ServiceEntryWow64, mov_edx);
  }

  // setup the thunk
  SetInternalThunk(&full_local_thunk->internal_thunk, GetInternalThunkSize(),
                   remote_thunk, interceptor_);

  size_t thunk_size = GetThunkSize();

  // copy the local thunk buffer to the child
  SIZE_T written;
  if (!::WriteProcessMemory(process_, remote_thunk, local_thunk, thunk_size,
                            &written)) {
    return STATUS_UNSUCCESSFUL;
  }

  if (thunk_size != written)
    return STATUS_UNSUCCESSFUL;

  // and now change the function to intercept, on the child
  if (ntdll_base_) {
    // running a unit test
    if (!::WriteProcessMemory(process_, target_, &intercepted_code,
                              bytes_to_write, &written))
      return STATUS_UNSUCCESSFUL;
  } else {
    if (!WriteProtectedChildMemory(process_, target_, &intercepted_code,
                                   bytes_to_write))
      return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

bool ServiceResolverThunk::SaveOriginalFunction(void* local_thunk,
                                                void* remote_thunk) {
  ServiceEntryWow64 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kJmp32 == function_code.mov_eax) {
    // Plain old entry point patch. The relative jump address follows it.
    ULONG relative = function_code.service_id;

    // First, fix our copy of their patch.
    relative += reinterpret_cast<ULONG>(target_) -
                reinterpret_cast<ULONG>(remote_thunk);

    function_code.service_id = relative;

    // And now, remember how to re-patch it.
    ServiceFullThunk* full_thunk =
        reinterpret_cast<ServiceFullThunk*>(remote_thunk);

    const ULONG kJmp32Size = 5;

    relative_jump_ = reinterpret_cast<ULONG>(&full_thunk->internal_thunk) -
                     reinterpret_cast<ULONG>(target_) - kJmp32Size;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

bool ServiceResolverThunk::VerifyJumpTargetForTesting(
    void* thunk_storage) const {
  const size_t kJmp32Size = 5;
  ServiceEntryWow64* patched = static_cast<ServiceEntryWow64*>(target_);
  if (kJmp32 != patched->mov_eax) {
    return false;
  }

  ULONG source_addr = reinterpret_cast<ULONG>(target_);
  ULONG target_addr = reinterpret_cast<ULONG>(thunk_storage);
  return target_addr + kMaxServiceSize - kJmp32Size - source_addr ==
         patched->service_id;
}

}  // namespace sandbox
