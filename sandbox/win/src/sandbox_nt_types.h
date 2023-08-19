// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_NT_TYPES_H_
#define SANDBOX_WIN_SRC_SANDBOX_NT_TYPES_H_

#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// clang-format off
struct NtExports {
  bool                                   Initialized;
  NtAllocateVirtualMemoryFunction        AllocateVirtualMemory;
  NtCreateFileFunction                   CreateFile;
  NtCreateSectionFunction                CreateSection;
  NtCloseFunction                        Close;
  NtDuplicateObjectFunction              DuplicateObject;
  NtFreeVirtualMemoryFunction            FreeVirtualMemory;
  NtMapViewOfSectionFunction             MapViewOfSection;
  NtOpenThreadFunction                   OpenThread;
  NtOpenProcessTokenExFunction           OpenProcessTokenEx;
  NtProtectVirtualMemoryFunction         ProtectVirtualMemory;
  NtQueryAttributesFileFunction          QueryAttributesFile;
  NtQueryFullAttributesFileFunction      QueryFullAttributesFile;
  NtQueryInformationProcessFunction      QueryInformationProcess;
  NtQueryObjectFunction                  QueryObject;
  NtQuerySectionFunction                 QuerySection;
  NtQueryVirtualMemoryFunction           QueryVirtualMemory;
  NtSetInformationFileFunction           SetInformationFile;
  NtSignalAndWaitForSingleObjectFunction SignalAndWaitForSingleObject;
  NtUnmapViewOfSectionFunction           UnmapViewOfSection;
  NtWaitForSingleObjectFunction          WaitForSingleObject;
  RtlAllocateHeapFunction                RtlAllocateHeap;
  RtlAnsiStringToUnicodeStringFunction   RtlAnsiStringToUnicodeString;
  RtlCompareUnicodeStringFunction        RtlCompareUnicodeString;
  RtlCreateHeapFunction                  RtlCreateHeap;
  RtlDestroyHeapFunction                 RtlDestroyHeap;
  RtlFreeHeapFunction                    RtlFreeHeap;
  RtlNtStatusToDosErrorFunction          RtlNtStatusToDosError;
  _strnicmpFunction                      _strnicmp;
  strlenFunction                         strlen;
  wcslenFunction                         wcslen;
  memcpyFunction                         memcpy;
};
// clang-format on

// This is the value used for the ntdll level allocator.
enum AllocationType {
  NT_ALLOC,
  NT_PAGE
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_NT_TYPES_H_
