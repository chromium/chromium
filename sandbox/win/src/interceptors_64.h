// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_INTERCEPTORS_64_H_
#define SANDBOX_WIN_SRC_INTERCEPTORS_64_H_

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtMapViewOfSection64(HANDLE section,
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
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtUnmapViewOfSection64(HANDLE process,
                                                               PVOID base);

// -----------------------------------------------------------------------
// Interceptors without IPC.

// Interception of NtSetInformationThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtSetInformationThread64(HANDLE thread,
                               THREADINFOCLASS thread_info_class,
                               PVOID thread_information,
                               ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenThreadToken64(HANDLE thread,
                          ACCESS_MASK desired_access,
                          BOOLEAN open_as_self,
                          PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenThreadTokenEx64(HANDLE thread,
                            ACCESS_MASK desired_access,
                            BOOLEAN open_as_self,
                            ULONG handle_attributes,
                            PHANDLE token);

// -----------------------------------------------------------------------
// Interceptors handled by the file system dispatcher.

// Interception of NtCreateFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateFile64(PHANDLE file,
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
TargetNtOpenFile64(PHANDLE file,
                   ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes,
                   PIO_STATUS_BLOCK io_status,
                   ULONG sharing,
                   ULONG options);

// Interception of NtQueryAtttributesFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtQueryAttributesFile64(POBJECT_ATTRIBUTES object_attributes,
                              PFILE_BASIC_INFORMATION file_attributes);

// Interception of NtQueryFullAtttributesFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtQueryFullAttributesFile64(
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes);

// Interception of NtSetInformationFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtSetInformationFile64(HANDLE file,
                             PIO_STATUS_BLOCK io_status,
                             PVOID file_information,
                             ULONG length,
                             FILE_INFORMATION_CLASS file_information_class);

// -----------------------------------------------------------------------
// Interceptors handled by the process-thread dispatcher.

// Interception of NtOpenThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenThread64(PHANDLE thread,
                     ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes,
                     PCLIENT_ID client_id);

// Interception of NtOpenProcess on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcess64(PHANDLE process,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PCLIENT_ID client_id);

// Interception of NtOpenProcessToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessToken64(HANDLE process,
                           ACCESS_MASK desired_access,
                           PHANDLE token);

// Interception of NtOpenProcessTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessTokenEx64(HANDLE process,
                             ACCESS_MASK desired_access,
                             ULONG handle_attributes,
                             PHANDLE token);

// Interception of CreateThread in kernel32.dll.
SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateThread64(LPSECURITY_ATTRIBUTES thread_attributes,
                     SIZE_T stack_size,
                     LPTHREAD_START_ROUTINE start_address,
                     PVOID parameter,
                     DWORD creation_flags,
                     LPDWORD thread_id);

// -----------------------------------------------------------------------
// Interceptors handled by the process mitigations win32k lockdown code.

// Interceptor for the GdiDllInitialize function.
SANDBOX_INTERCEPT BOOL WINAPI TargetGdiDllInitialize64(HANDLE dll,
                                                       DWORD reason);

// Interceptor for the GetStockObject function.
SANDBOX_INTERCEPT HGDIOBJ WINAPI TargetGetStockObject64(int object);

// Interceptor for the RegisterClassW function.
SANDBOX_INTERCEPT ATOM WINAPI TargetRegisterClassW64(const WNDCLASS* wnd_class);

// -----------------------------------------------------------------------
// Interceptors handled by the signed process code.

// Interception of NtCreateSection on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateSection64(PHANDLE section_handle,
                        ACCESS_MASK desired_access,
                        POBJECT_ATTRIBUTES object_attributes,
                        PLARGE_INTEGER maximum_size,
                        ULONG section_page_protection,
                        ULONG allocation_attributes,
                        HANDLE file_handle);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_INTERCEPTORS_64_H_
