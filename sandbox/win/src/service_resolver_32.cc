// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include <stddef.h>

#include <memory>

#include "base/bit_cast.h"
#include "sandbox/win/src/win_utils.h"

namespace {
#pragma pack(push, 1)

const BYTE kMovEax = 0xB8;
const BYTE kMovEdx = 0xBA;
const USHORT kMovEdxEsp = 0xD48B;
const USHORT kCallPtrEdx = 0x12FF;
const USHORT kCallEdx = 0xD2FF;
const BYTE kCallEip = 0xE8;
const BYTE kRet = 0xC2;
const BYTE kRet2 = 0xC3;
const USHORT kJmpEdx = 0xE2FF;
const USHORT kXorEcx = 0xC933;
const ULONG kLeaEdx = 0x0424548D;
const ULONG kCallFs1 = 0xC015FF64;
const USHORT kCallFs2 = 0;
const BYTE kCallFs3 = 0;
const BYTE kAddEsp1 = 0x83;
const USHORT kAddEsp2 = 0x4C4;
const BYTE kJmp32 = 0xE9;
const USHORT kSysenter = 0x340F;

// Service code for 32 bit systems.
// NOTE: on win2003 "call dword ptr [edx]" is "call edx".
struct ServiceEntry {
  // This struct contains roughly the following code:
  // 00 mov     eax,25h
  // 05 mov     edx,offset SharedUserData!SystemCallStub (7ffe0300)
  // 0a call    dword ptr [edx]
  // 0c ret     2Ch
  // 0f nop
  BYTE mov_eax;         // = B8
  ULONG service_id;
  BYTE mov_edx;         // = BA
  ULONG stub;
  USHORT call_ptr_edx;  // = FF 12
  BYTE ret;             // = C2
  USHORT num_params;
  BYTE nop;
};

// Service code for 32 bit Windows 8.
struct ServiceEntryW8 {
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

// Service code for a 32 bit process running on a 64 bit os.
struct Wow64Entry {
  // This struct may contain one of two versions of code:
  // 1. For XP, Vista and 2K3:
  // 00 b825000000      mov     eax, 25h
  // 05 33c9            xor     ecx, ecx
  // 07 8d542404        lea     edx, [esp + 4]
  // 0b 64ff15c0000000  call    dword ptr fs:[0C0h]
  // 12 c22c00          ret     2Ch
  //
  // 2. For Windows 7:
  // 00 b825000000      mov     eax, 25h
  // 05 33c9            xor     ecx, ecx
  // 07 8d542404        lea     edx, [esp + 4]
  // 0b 64ff15c0000000  call    dword ptr fs:[0C0h]
  // 12 83c404          add     esp, 4
  // 15 c22c00          ret     2Ch
  //
  // So we base the structure on the bigger one:
  BYTE mov_eax;         // = B8
  ULONG service_id;
  USHORT xor_ecx;       // = 33 C9
  ULONG lea_edx;        // = 8D 54 24 04
  ULONG call_fs1;       // = 64 FF 15 C0
  USHORT call_fs2;      // = 00 00
  BYTE call_fs3;        // = 00
  BYTE add_esp1;        // = 83             or ret
  USHORT add_esp2;      // = C4 04          or num_params
  BYTE ret;             // = C2
  USHORT num_params;
};

// Service code for a 32 bit process running on 64 bit Windows 8.
struct Wow64EntryW8 {
  // 00 b825000000      mov     eax, 25h
  // 05 64ff15c0000000  call    dword ptr fs:[0C0h]
  // 0b c22c00          ret     2Ch
  // 0f 90              nop
  BYTE mov_eax;         // = B8
  ULONG service_id;
  ULONG call_fs1;       // = 64 FF 15 C0
  USHORT call_fs2;      // = 00 00
  BYTE call_fs3;        // = 00
  BYTE ret;             // = C2
  USHORT num_params;
  BYTE nop;
};

// Service code for a 32 bit process running on 64 bit Windows 10.
struct Wow64EntryW10 {
  // 00 b828000000      mov     eax, 28h
  // 05 bab0d54877      mov     edx, 7748D5B0h
  // 09 ffd2            call    edx
  // 0b c22800          ret     28h
  BYTE mov_eax;         // = B8
  ULONG service_id;
  BYTE mov_edx;         // = BA
  ULONG mov_edx_param;
  USHORT call_edx;      // = FF D2
  BYTE ret;             // = C2
  USHORT num_params;
};

// Make sure that relaxed patching works as expected.
const size_t kMinServiceSize = offsetof(ServiceEntry, ret);
static_assert(sizeof(ServiceEntryW8) >= kMinServiceSize,
              "wrong service length");
static_assert(sizeof(Wow64Entry) >= kMinServiceSize, "wrong service length");
static_assert(sizeof(Wow64EntryW8) >= kMinServiceSize, "wrong service length");

struct ServiceFullThunk {
  union {
    ServiceEntry original;
    ServiceEntryW8 original_w8;
    Wow64Entry wow_64;
    Wow64EntryW8 wow_64_w8;
  };
  int internal_thunk;  // Dummy member to the beginning of the internal thunk.
};

#pragma pack(pop)

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
  std::unique_ptr<char[]> thunk_buffer(new char[thunk_bytes]);
  ServiceFullThunk* thunk =
      reinterpret_cast<ServiceFullThunk*>(thunk_buffer.get());

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
  ServiceEntry function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kMovEdx != function_code.mov_edx ||
      (kCallPtrEdx != function_code.call_ptr_edx &&
       kCallEdx != function_code.call_ptr_edx) ||
      kRet != function_code.ret) {
    return false;
  }

  // Find the system call pointer if we don't already have it.
  if (kCallEdx != function_code.call_ptr_edx) {
    DWORD ki_system_call;
    if (!::ReadProcessMemory(process_,
                             base::bit_cast<const void*>(function_code.stub),
                             &ki_system_call, sizeof(ki_system_call), &read)) {
      return false;
    }

    if (sizeof(ki_system_call) != read)
      return false;

    HMODULE module_1, module_2;
    // last check, call_stub should point to a KiXXSystemCall function on ntdll
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           base::bit_cast<const wchar_t*>(ki_system_call),
                           &module_1)) {
      return false;
    }

    if (ntdll_base_) {
      // This path is only taken when running the unit tests. We want to be
      // able to patch a buffer in memory, so target_ is not inside ntdll.
      module_2 = ntdll_base_;
    } else {
      if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<const wchar_t*>(target_),
                             &module_2))
        return false;
    }

    if (module_1 != module_2)
      return false;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  ServiceEntry intercepted_code;
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
  intercepted_code.stub =
      base::bit_cast<ULONG>(&full_remote_thunk->internal_thunk);
  intercepted_code.call_ptr_edx = kJmpEdx;
  bytes_to_write = kMinServiceSize;

  if (relative_jump_) {
    intercepted_code.mov_eax = kJmp32;
    intercepted_code.service_id = relative_jump_;
    bytes_to_write = offsetof(ServiceEntry, mov_edx);
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
  ServiceEntry function_code;
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
    relative +=
        base::bit_cast<ULONG>(target_) - base::bit_cast<ULONG>(remote_thunk);

    function_code.service_id = relative;

    // And now, remember how to re-patch it.
    ServiceFullThunk* full_thunk =
        reinterpret_cast<ServiceFullThunk*>(remote_thunk);

    const ULONG kJmp32Size = 5;

    relative_jump_ = base::bit_cast<ULONG>(&full_thunk->internal_thunk) -
                     base::bit_cast<ULONG>(target_) - kJmp32Size;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

bool Wow64ResolverThunk::IsFunctionAService(void* local_thunk) const {
  Wow64Entry function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kXorEcx != function_code.xor_ecx ||
      kLeaEdx != function_code.lea_edx || kCallFs1 != function_code.call_fs1 ||
      kCallFs2 != function_code.call_fs2 ||
      kCallFs3 != function_code.call_fs3) {
    return false;
  }

  if ((kAddEsp1 == function_code.add_esp1 &&
       kAddEsp2 == function_code.add_esp2 && kRet == function_code.ret) ||
      kRet == function_code.add_esp1) {
    // Save the verified code
    memcpy(local_thunk, &function_code, sizeof(function_code));
    return true;
  }

  return false;
}

bool Wow64W8ResolverThunk::IsFunctionAService(void* local_thunk) const {
  Wow64EntryW8 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read)) {
    return false;
  }

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kCallFs1 != function_code.call_fs1 ||
      kCallFs2 != function_code.call_fs2 ||
      kCallFs3 != function_code.call_fs3 || kRet != function_code.ret) {
    return false;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));
  return true;
}

bool Win8ResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceEntryW8 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
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

bool Wow64W10ResolverThunk::IsFunctionAService(void* local_thunk) const {
  Wow64EntryW10 function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
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

}  // namespace sandbox
