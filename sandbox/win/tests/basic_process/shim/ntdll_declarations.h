// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_NTDLL_DECLARATIONS_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_NTDLL_DECLARATIONS_H_

#include <windows.h>
#include <winternl.h>

extern "C" {

using PLDR_DLL_NOTIFICATION_FUNCTION =
    VOID(CALLBACK*)(ULONG notification_reason,
                    const VOID* notification_data,
                    PVOID context);

NTSTATUS WINAPI
LdrRegisterDllNotification(ULONG flags,
                           PLDR_DLL_NOTIFICATION_FUNCTION notification_function,
                           PVOID context,
                           PVOID* cookie);

using SECTION_INFORMATION_CLASS = int;
NTSTATUS WINAPI
NtQuerySection(HANDLE SectionHandle,
               SECTION_INFORMATION_CLASS SectionInformationClass,
               PVOID SectionInformation,
               ULONG SectionInformationLength,
               PULONG ReturnLength);

NTSTATUS WINAPI
RtlFormatCurrentUserKeyPath(_Out_ PUNICODE_STRING CurrentUserKeyPath);

NTSYSAPI NTSTATUS NTAPI NtCreateKey(_Out_ PHANDLE KeyHandle,
                                    _In_ ACCESS_MASK DesiredAccess,
                                    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
                                    ULONG TitleIndex,
                                    _In_opt_ PUNICODE_STRING Class,
                                    _In_ ULONG CreateOptions,
                                    _Out_opt_ PULONG Disposition);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenKeyEx(_Out_ PHANDLE KeyHandle,
            _In_ ACCESS_MASK DesiredAccess,
            _In_ POBJECT_ATTRIBUTES ObjectAttributes,
            _In_ ULONG OpenOptions);

typedef enum _KEY_VALUE_INFORMATION_CLASS {
  KeyValueFullInformation = 1
} KEY_VALUE_INFORMATION_CLASS,
    *PKEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_VALUE_FULL_INFORMATION {
  ULONG TitleIndex;
  ULONG Type;
  ULONG DataOffset;
  ULONG DataLength;
  ULONG NameLength;
  WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;
NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryValueKey(_In_ HANDLE KeyHandle,
                _In_ PCUNICODE_STRING ValueName,
                _In_ KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                _Out_writes_bytes_to_opt_(Length, *ResultLength)
                    PVOID KeyValueInformation,
                _In_ ULONG Length,
                _Out_ PULONG ResultLength);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetValueKey(_In_ HANDLE KeyHandle,
              _In_ PCUNICODE_STRING ValueName,
              _In_opt_ ULONG TitleIndex,
              _In_ ULONG Type,
              _In_reads_bytes_opt_(DataSize) PVOID Data,
              _In_ ULONG DataSize);

}  // extern "C"

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_NTDLL_DECLARATIONS_H_
