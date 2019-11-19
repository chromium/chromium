// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SANDBOX_NT_TYPES_H__
#define SANDBOX_SRC_SANDBOX_NT_TYPES_H__

#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

struct NtExports {
  NtAllocateVirtualMemoryFunction        AllocateVirtualMemory;
  NtCloseFunction                        Close;
  NtDuplicateObjectFunction              DuplicateObject;
  NtFreeVirtualMemoryFunction            FreeVirtualMemory;
  NtMapViewOfSectionFunction             MapViewOfSection;
  NtProtectVirtualMemoryFunction         ProtectVirtualMemory;
  NtQueryInformationProcessFunction      QueryInformationProcess;
  NtQueryObjectFunction                  QueryObject;
  NtQuerySectionFunction                 QuerySection;
  NtQueryVirtualMemoryFunction           QueryVirtualMemory;
  NtUnmapViewOfSectionFunction           UnmapViewOfSection;
  NtSignalAndWaitForSingleObjectFunction SignalAndWaitForSingleObject;
  NtWaitForSingleObjectFunction          WaitForSingleObject;
  RtlAllocateHeapFunction                RtlAllocateHeap;
  RtlAnsiStringToUnicodeStringFunction   RtlAnsiStringToUnicodeString;
  RtlCompareUnicodeStringFunction        RtlCompareUnicodeString;
  RtlCreateHeapFunction                  RtlCreateHeap;
  RtlCreateUserThreadFunction            RtlCreateUserThread;
  RtlDestroyHeapFunction                 RtlDestroyHeap;
  RtlFreeHeapFunction                    RtlFreeHeap;
  _strnicmpFunction                      _strnicmp;
  strlenFunction                         strlen;
  wcslenFunction                         wcslen;
  memcpyFunction                         memcpy;
};

// This is the value used for the ntdll level allocator.
enum AllocationType {
  NT_ALLOC,
  NT_PAGE
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_NT_TYPES_H__
