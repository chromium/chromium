// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/heap_helper.h"

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "base/win/windows_version.h"

namespace sandbox {
namespace {
#pragma pack(1)

// These are undocumented, but readily found on the internet.
constexpr DWORD kHeapClass8 = 0x00008000;  // CSR port heap
constexpr DWORD kHeapClassMask = 0x0000f000;

constexpr DWORD kHeapSegmentSignature = 0xffeeffee;
constexpr DWORD kHeapSignature = 0xeeffeeff;

typedef struct _HEAP_ENTRY {
  PVOID Data1;
  PVOID Data2;
} HEAP_ENTRY, *PHEAP_ENTRY;

// The _HEAP struct is not documented, so char arrays are used to space out the
// struct of the fields that are not relevant. However, this spacing is
// different because of the different pointer widths between 32 and 64-bit.
// So 32 and 64 bit structs are defined.
struct _HEAP_32 {
  HEAP_ENTRY HeapEntry;
  DWORD SegmentSignature;
  DWORD SegmentFlags;
  LIST_ENTRY SegmentListEntry;
  struct _HEAP_32* Heap;
  char Unknown0[0x24];
  // Offset 0x40
  DWORD Flags;
  // Offset 0x60
  char Unknown1[0x1c];
  DWORD Signature;
  // Other stuff that is not relevant.
};

struct _HEAP_64 {
  HEAP_ENTRY HeapEntry;
  DWORD SegmentSignature;
  DWORD SegmentFlags;
  LIST_ENTRY SegmentListEntry;
  struct _HEAP_64* Heap;
  char Unknown0[0x40];
  // Offset 0x70
  DWORD Flags;
  // Offset 0x98
  char Unknown1[0x24];
  DWORD Signature;
  // Other stuff that is not relevant.
};

#if defined(_WIN64)
using _HEAP = _HEAP_64;
#else   // defined(_WIN64)
using _HEAP = _HEAP_32;
#endif  // defined(_WIN64)

bool ValidateHeap(_HEAP* heap) {
  if (heap->SegmentSignature != kHeapSegmentSignature)
    return false;
  if (heap->Heap != heap)
    return false;
  if (heap->Signature != kHeapSignature)
    return false;
  return true;
}

}  // namespace

bool HeapFlags(HANDLE handle, DWORD* flags) {
  if (!handle || !flags) {
    // This is an error.
    return false;
  }
  _HEAP* heap = reinterpret_cast<_HEAP*>(handle);
  if (!ValidateHeap(heap)) {
    DLOG(ERROR) << "unable to validate heap";
    return false;
  }
  *flags = heap->Flags;
  return true;
}

HANDLE FindCsrPortHeap() {
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    // This functionality has not been verified on versions before Win10.
    return nullptr;
  }
  DWORD number_of_heaps = ::GetProcessHeaps(0, nullptr);
  std::unique_ptr<HANDLE[]> all_heaps(new HANDLE[number_of_heaps]);
  if (::GetProcessHeaps(number_of_heaps, all_heaps.get()) != number_of_heaps)
    return nullptr;

  // Search for the CSR port heap handle, identified purely based on flags.
  HANDLE csr_port_heap = nullptr;
  for (size_t i = 0; i < number_of_heaps; ++i) {
    HANDLE handle = all_heaps[i];
    DWORD flags = 0;
    if (!HeapFlags(handle, &flags)) {
      DLOG(ERROR) << "Unable to get flags for this heap";
      continue;
    }
    if ((flags & kHeapClassMask) == kHeapClass8) {
      if (csr_port_heap) {
        DLOG(ERROR) << "Found multiple suitable CSR Port heaps";
        return nullptr;
      }
      csr_port_heap = handle;
    }
  }
  return csr_port_heap;
}

}  // namespace sandbox
