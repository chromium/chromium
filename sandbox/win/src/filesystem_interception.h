// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_FILESYSTEM_INTERCEPTION_H_
#define SANDBOX_WIN_SRC_FILESYSTEM_INTERCEPTION_H_

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

// Interception of NtCreateFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateFile(NtCreateFileFunction orig_CreateFile,
                   PHANDLE file,
                   ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes,
                   PIO_STATUS_BLOCK io_status,
                   PLARGE_INTEGER allocation_size,
                   ULONG file_attributes,
                   ULONG sharing,
                   ULONG disposition,
                   ULONG options,
                   PVOID ea_buffer,
                   ULONG ea_length);

// Interception of NtOpenFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenFile(NtOpenFileFunction orig_OpenFile,
                 PHANDLE file,
                 ACCESS_MASK desired_access,
                 POBJECT_ATTRIBUTES object_attributes,
                 PIO_STATUS_BLOCK io_status,
                 ULONG sharing,
                 ULONG options);

// Interception of NtQueryAtttributesFile on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtQueryAttributesFile(NtQueryAttributesFileFunction orig_QueryAttributes,
                            POBJECT_ATTRIBUTES object_attributes,
                            PFILE_BASIC_INFORMATION file_attributes);

// Interception of NtQueryFullAtttributesFile on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtQueryFullAttributesFile(
    NtQueryFullAttributesFileFunction orig_QueryAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes);

// Interception of NtSetInformationFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtSetInformationFile(NtSetInformationFileFunction orig_SetInformationFile,
                           HANDLE file,
                           PIO_STATUS_BLOCK io_status,
                           PVOID file_information,
                           ULONG length,
                           FILE_INFORMATION_CLASS file_information_class);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_FILESYSTEM_INTERCEPTION_H_
