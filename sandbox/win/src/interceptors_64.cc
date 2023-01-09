// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/interceptors_64.h"

#include "sandbox/win/src/filesystem_interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/named_pipe_interception.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/process_mitigations_win32k_interception.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/signed_interception.h"
#include "sandbox/win/src/target_interceptions.h"

namespace sandbox {

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
                               THREADINFOCLASS thread_info_class,
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
