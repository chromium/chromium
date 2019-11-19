// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/interceptors_64.h"

#include "sandbox/win/src/filesystem_interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/named_pipe_interception.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/process_mitigations_win32k_interception.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/registry_interception.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/signed_interception.h"
#include "sandbox/win/src/sync_interception.h"
#include "sandbox/win/src/target_interceptions.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;
SANDBOX_INTERCEPT OriginalFunctions g_originals;

NTSTATUS WINAPI TargetNtMapViewOfSection64(HANDLE section,
                                           HANDLE process,
                                           PVOID* base,
                                           ULONG_PTR zero_bits,
                                           SIZE_T commit_size,
                                           PLARGE_INTEGER offset,
                                           PSIZE_T view_size,
                                           SECTION_INHERIT inherit,
                                           ULONG allocation_type,
                                           ULONG protect) {
  NtMapViewOfSectionFunction orig_fn =
      reinterpret_cast<NtMapViewOfSectionFunction>(
          g_originals[MAP_VIEW_OF_SECTION_ID]);

  return TargetNtMapViewOfSection(orig_fn, section, process, base, zero_bits,
                                  commit_size, offset, view_size, inherit,
                                  allocation_type, protect);
}

NTSTATUS WINAPI TargetNtUnmapViewOfSection64(HANDLE process, PVOID base) {
  NtUnmapViewOfSectionFunction orig_fn =
      reinterpret_cast<NtUnmapViewOfSectionFunction>(
          g_originals[UNMAP_VIEW_OF_SECTION_ID]);
  return TargetNtUnmapViewOfSection(orig_fn, process, base);
}

// -----------------------------------------------------------------------

NTSTATUS WINAPI
TargetNtSetInformationThread64(HANDLE thread,
                               NT_THREAD_INFORMATION_CLASS thread_info_class,
                               PVOID thread_information,
                               ULONG thread_information_bytes) {
  NtSetInformationThreadFunction orig_fn =
      reinterpret_cast<NtSetInformationThreadFunction>(
          g_originals[SET_INFORMATION_THREAD_ID]);
  return TargetNtSetInformationThread(orig_fn, thread, thread_info_class,
                                      thread_information,
                                      thread_information_bytes);
}

NTSTATUS WINAPI TargetNtOpenThreadToken64(HANDLE thread,
                                          ACCESS_MASK desired_access,
                                          BOOLEAN open_as_self,
                                          PHANDLE token) {
  NtOpenThreadTokenFunction orig_fn =
      reinterpret_cast<NtOpenThreadTokenFunction>(
          g_originals[OPEN_THREAD_TOKEN_ID]);
  return TargetNtOpenThreadToken(orig_fn, thread, desired_access, open_as_self,
                                 token);
}

NTSTATUS WINAPI TargetNtOpenThreadTokenEx64(HANDLE thread,
                                            ACCESS_MASK desired_access,
                                            BOOLEAN open_as_self,
                                            ULONG handle_attributes,
                                            PHANDLE token) {
  NtOpenThreadTokenExFunction orig_fn =
      reinterpret_cast<NtOpenThreadTokenExFunction>(
          g_originals[OPEN_THREAD_TOKEN_EX_ID]);
  return TargetNtOpenThreadTokenEx(orig_fn, thread, desired_access,
                                   open_as_self, handle_attributes, token);
}

// -----------------------------------------------------------------------

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
                     ULONG ea_length) {
  NtCreateFileFunction orig_fn =
      reinterpret_cast<NtCreateFileFunction>(g_originals[CREATE_FILE_ID]);
  return TargetNtCreateFile(orig_fn, file, desired_access, object_attributes,
                            io_status, allocation_size, file_attributes,
                            sharing, disposition, options, ea_buffer,
                            ea_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenFile64(PHANDLE file,
                   ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes,
                   PIO_STATUS_BLOCK io_status,
                   ULONG sharing,
                   ULONG options) {
  NtOpenFileFunction orig_fn =
      reinterpret_cast<NtOpenFileFunction>(g_originals[OPEN_FILE_ID]);
  return TargetNtOpenFile(orig_fn, file, desired_access, object_attributes,
                          io_status, sharing, options);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtQueryAttributesFile64(POBJECT_ATTRIBUTES object_attributes,
                              PFILE_BASIC_INFORMATION file_attributes) {
  NtQueryAttributesFileFunction orig_fn =
      reinterpret_cast<NtQueryAttributesFileFunction>(
          g_originals[QUERY_ATTRIB_FILE_ID]);
  return TargetNtQueryAttributesFile(orig_fn, object_attributes,
                                     file_attributes);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtQueryFullAttributesFile64(
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes) {
  NtQueryFullAttributesFileFunction orig_fn =
      reinterpret_cast<NtQueryFullAttributesFileFunction>(
          g_originals[QUERY_FULL_ATTRIB_FILE_ID]);
  return TargetNtQueryFullAttributesFile(orig_fn, object_attributes,
                                         file_attributes);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtSetInformationFile64(HANDLE file,
                             PIO_STATUS_BLOCK io_status,
                             PVOID file_information,
                             ULONG length,
                             FILE_INFORMATION_CLASS file_information_class) {
  NtSetInformationFileFunction orig_fn =
      reinterpret_cast<NtSetInformationFileFunction>(
          g_originals[SET_INFO_FILE_ID]);
  return TargetNtSetInformationFile(orig_fn, file, io_status, file_information,
                                    length, file_information_class);
}

// -----------------------------------------------------------------------

SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateNamedPipeW64(LPCWSTR pipe_name,
                         DWORD open_mode,
                         DWORD pipe_mode,
                         DWORD max_instance,
                         DWORD out_buffer_size,
                         DWORD in_buffer_size,
                         DWORD default_timeout,
                         LPSECURITY_ATTRIBUTES security_attributes) {
  CreateNamedPipeWFunction orig_fn = reinterpret_cast<CreateNamedPipeWFunction>(
      g_originals[CREATE_NAMED_PIPE_ID]);
  return TargetCreateNamedPipeW(orig_fn, pipe_name, open_mode, pipe_mode,
                                max_instance, out_buffer_size, in_buffer_size,
                                default_timeout, security_attributes);
}

// -----------------------------------------------------------------------

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenThread64(PHANDLE thread,
                     ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes,
                     PCLIENT_ID client_id) {
  NtOpenThreadFunction orig_fn =
      reinterpret_cast<NtOpenThreadFunction>(g_originals[OPEN_THREAD_ID]);
  return TargetNtOpenThread(orig_fn, thread, desired_access, object_attributes,
                            client_id);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcess64(PHANDLE process,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PCLIENT_ID client_id) {
  NtOpenProcessFunction orig_fn =
      reinterpret_cast<NtOpenProcessFunction>(g_originals[OPEN_PROCESS_ID]);
  return TargetNtOpenProcess(orig_fn, process, desired_access,
                             object_attributes, client_id);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessToken64(HANDLE process,
                           ACCESS_MASK desired_access,
                           PHANDLE token) {
  NtOpenProcessTokenFunction orig_fn =
      reinterpret_cast<NtOpenProcessTokenFunction>(
          g_originals[OPEN_PROCESS_TOKEN_ID]);
  return TargetNtOpenProcessToken(orig_fn, process, desired_access, token);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessTokenEx64(HANDLE process,
                             ACCESS_MASK desired_access,
                             ULONG handle_attributes,
                             PHANDLE token) {
  NtOpenProcessTokenExFunction orig_fn =
      reinterpret_cast<NtOpenProcessTokenExFunction>(
          g_originals[OPEN_PROCESS_TOKEN_EX_ID]);
  return TargetNtOpenProcessTokenEx(orig_fn, process, desired_access,
                                    handle_attributes, token);
}

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
                       LPPROCESS_INFORMATION process_information) {
  CreateProcessWFunction orig_fn =
      reinterpret_cast<CreateProcessWFunction>(g_originals[CREATE_PROCESSW_ID]);
  return TargetCreateProcessW(
      orig_fn, application_name, command_line, process_attributes,
      thread_attributes, inherit_handles, flags, environment, current_directory,
      startup_info, process_information);
}

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
                       LPPROCESS_INFORMATION process_information) {
  CreateProcessAFunction orig_fn =
      reinterpret_cast<CreateProcessAFunction>(g_originals[CREATE_PROCESSA_ID]);
  return TargetCreateProcessA(
      orig_fn, application_name, command_line, process_attributes,
      thread_attributes, inherit_handles, flags, environment, current_directory,
      startup_info, process_information);
}

SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateThread64(LPSECURITY_ATTRIBUTES thread_attributes,
                     SIZE_T stack_size,
                     LPTHREAD_START_ROUTINE start_address,
                     PVOID parameter,
                     DWORD creation_flags,
                     LPDWORD thread_id) {
  CreateThreadFunction orig_fn =
      reinterpret_cast<CreateThreadFunction>(g_originals[CREATE_THREAD_ID]);
  return TargetCreateThread(orig_fn, thread_attributes, stack_size,
                            start_address, parameter, creation_flags,
                            thread_id);
}

// -----------------------------------------------------------------------

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateKey64(PHANDLE key,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    ULONG title_index,
                    PUNICODE_STRING class_name,
                    ULONG create_options,
                    PULONG disposition) {
  NtCreateKeyFunction orig_fn =
      reinterpret_cast<NtCreateKeyFunction>(g_originals[CREATE_KEY_ID]);
  return TargetNtCreateKey(orig_fn, key, desired_access, object_attributes,
                           title_index, class_name, create_options,
                           disposition);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenKey64(PHANDLE key,
                  ACCESS_MASK desired_access,
                  POBJECT_ATTRIBUTES object_attributes) {
  NtOpenKeyFunction orig_fn =
      reinterpret_cast<NtOpenKeyFunction>(g_originals[OPEN_KEY_ID]);
  return TargetNtOpenKey(orig_fn, key, desired_access, object_attributes);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenKeyEx64(PHANDLE key,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    ULONG open_options) {
  NtOpenKeyExFunction orig_fn =
      reinterpret_cast<NtOpenKeyExFunction>(g_originals[OPEN_KEY_EX_ID]);
  return TargetNtOpenKeyEx(orig_fn, key, desired_access, object_attributes,
                           open_options);
}

// -----------------------------------------------------------------------

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateEvent64(PHANDLE event_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      EVENT_TYPE event_type,
                      BOOLEAN initial_state) {
  NtCreateEventFunction orig_fn =
      reinterpret_cast<NtCreateEventFunction>(g_originals[CREATE_EVENT_ID]);
  return TargetNtCreateEvent(orig_fn, event_handle, desired_access,
                             object_attributes, event_type, initial_state);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenEvent64(PHANDLE event_handle,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes) {
  NtOpenEventFunction orig_fn =
      reinterpret_cast<NtOpenEventFunction>(g_originals[OPEN_EVENT_ID]);
  return TargetNtOpenEvent(orig_fn, event_handle, desired_access,
                           object_attributes);
}

// -----------------------------------------------------------------------

SANDBOX_INTERCEPT BOOL WINAPI TargetGdiDllInitialize64(HANDLE dll,
                                                       DWORD reason) {
  GdiDllInitializeFunction orig_fn =
      reinterpret_cast<GdiDllInitializeFunction>(g_originals[GDIINITIALIZE_ID]);
  return TargetGdiDllInitialize(orig_fn, dll, reason);
}

SANDBOX_INTERCEPT HGDIOBJ WINAPI TargetGetStockObject64(int object) {
  GetStockObjectFunction orig_fn =
      reinterpret_cast<GetStockObjectFunction>(g_originals[GETSTOCKOBJECT_ID]);
  return TargetGetStockObject(orig_fn, object);
}

SANDBOX_INTERCEPT ATOM WINAPI
TargetRegisterClassW64(const WNDCLASS* wnd_class) {
  RegisterClassWFunction orig_fn =
      reinterpret_cast<RegisterClassWFunction>(g_originals[REGISTERCLASSW_ID]);
  return TargetRegisterClassW(orig_fn, wnd_class);
}

SANDBOX_INTERCEPT BOOL WINAPI
TargetEnumDisplayMonitors64(HDC hdc,
                            LPCRECT clip_rect,
                            MONITORENUMPROC enum_function,
                            LPARAM data_pointer) {
  EnumDisplayMonitorsFunction orig_fn =
      reinterpret_cast<EnumDisplayMonitorsFunction>(
          g_originals[ENUMDISPLAYMONITORS_ID]);
  return TargetEnumDisplayMonitors(orig_fn, hdc, clip_rect, enum_function,
                                   data_pointer);
}

SANDBOX_INTERCEPT BOOL WINAPI
TargetEnumDisplayDevicesA64(LPCSTR device,
                            DWORD device_num,
                            PDISPLAY_DEVICEA display_device,
                            DWORD flags) {
  EnumDisplayDevicesAFunction orig_fn =
      reinterpret_cast<EnumDisplayDevicesAFunction>(
          g_originals[ENUMDISPLAYDEVICESA_ID]);
  return TargetEnumDisplayDevicesA(orig_fn, device, device_num, display_device,
                                   flags);
}

SANDBOX_INTERCEPT BOOL WINAPI
TargetGetMonitorInfoA64(HMONITOR monitor, LPMONITORINFO monitor_info) {
  GetMonitorInfoAFunction orig_fn = reinterpret_cast<GetMonitorInfoAFunction>(
      g_originals[GETMONITORINFOA_ID]);
  return TargetGetMonitorInfoA(orig_fn, monitor, monitor_info);
}

SANDBOX_INTERCEPT BOOL WINAPI
TargetGetMonitorInfoW64(HMONITOR monitor, LPMONITORINFO monitor_info) {
  GetMonitorInfoWFunction orig_fn = reinterpret_cast<GetMonitorInfoWFunction>(
      g_originals[GETMONITORINFOW_ID]);
  return TargetGetMonitorInfoW(orig_fn, monitor, monitor_info);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetSuggestedOPMProtectedOutputArraySize64(
    PUNICODE_STRING device_name,
    DWORD* suggested_output_array_size) {
  GetSuggestedOPMProtectedOutputArraySizeFunction orig_fn =
      reinterpret_cast<GetSuggestedOPMProtectedOutputArraySizeFunction>(
          g_originals[GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE_ID]);
  return TargetGetSuggestedOPMProtectedOutputArraySize(
      orig_fn, device_name, suggested_output_array_size);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetCreateOPMProtectedOutputs64(
    PUNICODE_STRING device_name,
    DXGKMDT_OPM_VIDEO_OUTPUT_SEMANTICS vos,
    DWORD protected_output_array_size,
    DWORD* num_output_handles,
    OPM_PROTECTED_OUTPUT_HANDLE* protected_output_array) {
  CreateOPMProtectedOutputsFunction orig_fn =
      reinterpret_cast<CreateOPMProtectedOutputsFunction>(
          g_originals[CREATEOPMPROTECTEDOUTPUTS_ID]);
  return TargetCreateOPMProtectedOutputs(
      orig_fn, device_name, vos, protected_output_array_size,
      num_output_handles, protected_output_array);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificate64(PUNICODE_STRING device_name,
                       DXGKMDT_CERTIFICATE_TYPE certificate_type,
                       BYTE* certificate,
                       ULONG certificate_length) {
  GetCertificateFunction orig_fn =
      reinterpret_cast<GetCertificateFunction>(g_originals[GETCERTIFICATE_ID]);
  return TargetGetCertificate(orig_fn, device_name, certificate_type,
                              certificate, certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateSize64(PUNICODE_STRING device_name,
                           DXGKMDT_CERTIFICATE_TYPE certificate_type,
                           ULONG* certificate_length) {
  GetCertificateSizeFunction orig_fn =
      reinterpret_cast<GetCertificateSizeFunction>(
          g_originals[GETCERTIFICATESIZE_ID]);
  return TargetGetCertificateSize(orig_fn, device_name, certificate_type,
                                  certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateByHandle64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                               DXGKMDT_CERTIFICATE_TYPE certificate_type,
                               BYTE* certificate,
                               ULONG certificate_length) {
  GetCertificateByHandleFunction orig_fn =
      reinterpret_cast<GetCertificateByHandleFunction>(
          g_originals[GETCERTIFICATE_ID]);
  return TargetGetCertificateByHandle(orig_fn, protected_output,
                                      certificate_type, certificate,
                                      certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetCertificateSizeByHandle64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                                   DXGKMDT_CERTIFICATE_TYPE certificate_type,
                                   ULONG* certificate_length) {
  GetCertificateSizeByHandleFunction orig_fn =
      reinterpret_cast<GetCertificateSizeByHandleFunction>(
          g_originals[GETCERTIFICATESIZE_ID]);
  return TargetGetCertificateSizeByHandle(orig_fn, protected_output,
                                          certificate_type, certificate_length);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetDestroyOPMProtectedOutput64(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output) {
  DestroyOPMProtectedOutputFunction orig_fn =
      reinterpret_cast<DestroyOPMProtectedOutputFunction>(
          g_originals[DESTROYOPMPROTECTEDOUTPUT_ID]);
  return TargetDestroyOPMProtectedOutput(orig_fn, protected_output);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetOPMInformation64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                          const DXGKMDT_OPM_GET_INFO_PARAMETERS* parameters,
                          DXGKMDT_OPM_REQUESTED_INFORMATION* requested_info) {
  GetOPMInformationFunction orig_fn =
      reinterpret_cast<GetOPMInformationFunction>(
          g_originals[GETOPMINFORMATION_ID]);
  return TargetGetOPMInformation(orig_fn, protected_output, parameters,
                                 requested_info);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetGetOPMRandomNumber64(OPM_PROTECTED_OUTPUT_HANDLE protected_output,
                           DXGKMDT_OPM_RANDOM_NUMBER* random_number) {
  GetOPMRandomNumberFunction orig_fn =
      reinterpret_cast<GetOPMRandomNumberFunction>(
          g_originals[GETOPMRANDOMNUMBER_ID]);
  return TargetGetOPMRandomNumber(orig_fn, protected_output, random_number);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetSetOPMSigningKeyAndSequenceNumbers64(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_ENCRYPTED_PARAMETERS* parameters) {
  SetOPMSigningKeyAndSequenceNumbersFunction orig_fn =
      reinterpret_cast<SetOPMSigningKeyAndSequenceNumbersFunction>(
          g_originals[SETOPMSIGNINGKEYANDSEQUENCENUMBERS_ID]);
  return TargetSetOPMSigningKeyAndSequenceNumbers(orig_fn, protected_output,
                                                  parameters);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI TargetConfigureOPMProtectedOutput64(
    OPM_PROTECTED_OUTPUT_HANDLE protected_output,
    const DXGKMDT_OPM_CONFIGURE_PARAMETERS* parameters,
    ULONG additional_parameters_size,
    const BYTE* additional_parameters) {
  ConfigureOPMProtectedOutputFunction orig_fn =
      reinterpret_cast<ConfigureOPMProtectedOutputFunction>(
          g_originals[CONFIGUREOPMPROTECTEDOUTPUT_ID]);
  return TargetConfigureOPMProtectedOutput(
      orig_fn, protected_output, parameters, additional_parameters_size,
      additional_parameters);
}

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateSection64(PHANDLE section_handle,
                        ACCESS_MASK desired_access,
                        POBJECT_ATTRIBUTES object_attributes,
                        PLARGE_INTEGER maximum_size,
                        ULONG section_page_protection,
                        ULONG allocation_attributes,
                        HANDLE file_handle) {
  NtCreateSectionFunction orig_fn =
      reinterpret_cast<NtCreateSectionFunction>(g_originals[CREATE_SECTION_ID]);
  return TargetNtCreateSection(
      orig_fn, section_handle, desired_access, object_attributes, maximum_size,
      section_page_protection, allocation_attributes, file_handle);
}

}  // namespace sandbox
