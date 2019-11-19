// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
                               NT_THREAD_INFORMATION_CLASS thread_info_class,
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
// Interceptors handled by the named pipe dispatcher.

// Interception of CreateNamedPipeW in kernel32.dll
SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateNamedPipeW64(LPCWSTR pipe_name,
                         DWORD open_mode,
                         DWORD pipe_mode,
                         DWORD max_instance,
                         DWORD out_buffer_size,
                         DWORD in_buffer_size,
                         DWORD default_timeout,
                         LPSECURITY_ATTRIBUTES security_attributes);

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

// Interception of CreateProcessW in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI
TargetCreateProcessW64(LPCWSTR application_name,
                       LPWSTR command_line,
                       LPSECURITY_ATTRIBUTES process_attributes,
                       LPSECURITY_ATTRIBUTES thread_attributes,
                       BOOL inherit_handles,
                       DWORD flags,
                       LPVOID environment,
                       LPCWSTR current_directory,
                       LPSTARTUPINFOW startup_info,
                       LPPROCESS_INFORMATION process_information);

// Interception of CreateProcessA in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI
TargetCreateProcessA64(LPCSTR application_name,
                       LPSTR command_line,
                       LPSECURITY_ATTRIBUTES process_attributes,
                       LPSECURITY_ATTRIBUTES thread_attributes,
                       BOOL inherit_handles,
                       DWORD flags,
                       LPVOID environment,
                       LPCSTR current_directory,
                       LPSTARTUPINFOA startup_info,
                       LPPROCESS_INFORMATION process_information);

// Interception of CreateThread in kernel32.dll.
SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateThread64(LPSECURITY_ATTRIBUTES thread_attributes,
                     SIZE_T stack_size,
                     LPTHREAD_START_ROUTINE start_address,
                     PVOID parameter,
                     DWORD creation_flags,
                     LPDWORD thread_id);

// -----------------------------------------------------------------------
// Interceptors handled by the registry dispatcher.

// Interception of NtCreateKey on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateKey64(PHANDLE key,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    ULONG title_index,
                    PUNICODE_STRING class_name,
                    ULONG create_options,
                    PULONG disposition);

// Interception of NtOpenKey on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenKey64(PHANDLE key,
                  ACCESS_MASK desired_access,
                  POBJECT_ATTRIBUTES object_attributes);

// Interception of NtOpenKeyEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenKeyEx64(PHANDLE key,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    ULONG open_options);

// -----------------------------------------------------------------------
// Interceptors handled by the sync dispatcher.

// Interception of NtCreateEvent/NtOpenEvent on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateEvent64(PHANDLE event_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      EVENT_TYPE event_type,
                      BOOLEAN initial_state);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenEvent64(PHANDLE event_handle,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes);

// -----------------------------------------------------------------------
// Interceptors handled by the process mitigations win32k lockdown code.

// Interceptor for the GdiDllInitialize function.
SANDBOX_INTERCEPT BOOL WINAPI TargetGdiDllInitialize64(HANDLE dll,
                                                       DWORD reason);

// Interceptor for the GetStockObject function.
SANDBOX_INTERCEPT HGDIOBJ WINAPI TargetGetStockObject64(int object);

// Interceptor for the RegisterClassW function.
SANDBOX_INTERCEPT ATOM WINAPI TargetRegisterClassW64(const WNDCLASS* wnd_class);

SANDBOX_INTERCEPT BOOL WINAPI
TargetEnumDisplayMonitors64(HDC hdc,
                            LPCRECT lprcClip,
                            MONITORENUMPROC lpfnEnum,
                            LPARAM dwData);

SANDBOX_INTERCEPT BOOL WINAPI
TargetEnumDisplayDevicesA64(LPCSTR lpDevice,
                            DWORD iDevNum,
                            PDISPLAY_DEVICEA lpDisplayDevice,
                            DWORD dwFlags);

SANDBOX_INTERCEPT BOOL WINAPI TargetGetMonitorInfoA64(HMONITOR hMonitor,
                                                      LPMONITORINFO lpmi);

SANDBOX_INTERCEPT BOOL WINAPI TargetGetMonitorInfoW64(HMONITOR hMonitor,
                                                      LPMONITORINFO lpmi);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetSuggestedOPMProtectedOutputArraySize64(
    PUNICODE_STRING device_name,
    DWORD* suggested_output_array_size);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetCreateOPMProtectedOutputs64(
    PUNICODE_STRING device_name,
    DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
    DWORD protected_output_array_size,
    DWORD* num_output_handles,
    OPM_PROTECTED_OUTPUT_HANDLE* protected_output_array);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificate64(PUNICODE_STRING device_name,
                       DXGKMDT_CERTIFICATE_TYPE certificate_type,
                       BYTE* certificate,
                       ULONG certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateSize64(PUNICODE_STRING device_name,
                           DXGKMDT_CERTIFICATE_TYPE certificate_type,
                           ULONG* certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateByHandle64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                               DXGKMDT_CERTIFICATE_TYPE certificate_type,
                               BYTE* certificate,
                               ULONG certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateSizeByHandle64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                                   DXGKMDT_CERTIFICATE_TYPE certificate_type,
                                   ULONG* certificate_length);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetDestroyOPMProtectedOutput64(OPM_PROTECTED_OUTPUT_HANDLE protected_output);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetOPMInformation64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                          const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
                          DXGKMDT_OPM_REQUESTED_INFORMATION* requested_info);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetOPMRandomNumber64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                           DXGKMDT_OPM_RANDOM_NUMBER* random_number);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetSetOPMSigningKeyAndSequenceNumbers64(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters);

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetConfigureOPMProtectedOutput64(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters);

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
