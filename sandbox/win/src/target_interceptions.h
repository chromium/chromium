// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_INTERCEPTIONS_H_
#define SANDBOX_WIN_SRC_TARGET_INTERCEPTIONS_H_

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtMapViewOfSection(NtMapViewOfSectionFunction orig_MapViewOfSection,
                         HANDLE section,
                         HANDLE process,
                         PVOID* base,
                         ULONG_PTR zero_bits,
                         SIZE_T commit_size,
                         PLARGE_INTEGER offset,
                         PSIZE_T view_size,
                         SECTION_INHERIT inherit,
                         ULONG allocation_type,
                         ULONG protect);

// Interception of NtUnmapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being unloaded, so we can clean up our interceptions.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtUnmapViewOfSection(NtUnmapViewOfSectionFunction orig_UnmapViewOfSection,
                           HANDLE process,
                           PVOID base);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_INTERCEPTIONS_H_
