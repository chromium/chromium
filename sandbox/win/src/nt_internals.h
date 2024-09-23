// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file holds definitions related to the ntdll API that are missing
// from <winternl.h>.

#ifndef SANDBOX_WIN_SRC_NT_INTERNALS_H__
#define SANDBOX_WIN_SRC_NT_INTERNALS_H__

#include <windows.h>

#include <stddef.h>
#include <winternl.h>

#define CURRENT_PROCESS ((HANDLE)-1)
#define CURRENT_THREAD ((HANDLE)-2)
#define NtCurrentProcess CURRENT_PROCESS

typedef NTSTATUS(WINAPI* NtCreateFileFunction)(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength);

typedef NTSTATUS(WINAPI* NtOpenFileFunction)(OUT PHANDLE FileHandle,
                                             IN ACCESS_MASK DesiredAccess,
                                             IN POBJECT_ATTRIBUTES
                                                 ObjectAttributes,
                                             OUT PIO_STATUS_BLOCK IoStatusBlock,
                                             IN ULONG ShareAccess,
                                             IN ULONG OpenOptions);

typedef NTSTATUS(WINAPI* NtCloseFunction)(IN HANDLE Handle);

typedef struct _FILE_RENAME_INFORMATION {
  BOOLEAN ReplaceIfExists;
  HANDLE RootDirectory;
  ULONG FileNameLength;
  WCHAR FileName[1];
} FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

typedef NTSTATUS(WINAPI* NtSetInformationFileFunction)(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass);

typedef struct FILE_BASIC_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef NTSTATUS(WINAPI* NtQueryAttributesFileFunction)(
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PFILE_BASIC_INFORMATION FileAttributes);

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef NTSTATUS(WINAPI* NtQueryFullAttributesFileFunction)(
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PFILE_NETWORK_OPEN_INFORMATION FileAttributes);

// -----------------------------------------------------------------------
// Sections

typedef NTSTATUS(WINAPI* NtCreateSectionFunction)(
    OUT PHANDLE SectionHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER MaximumSize OPTIONAL,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL);

typedef ULONG SECTION_INHERIT;
#define ViewShare 1
#define ViewUnmap 2

typedef NTSTATUS(WINAPI* NtMapViewOfSectionFunction)(
    IN HANDLE SectionHandle,
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN SIZE_T CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
    IN OUT PSIZE_T ViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Win32Protect);

typedef NTSTATUS(WINAPI* NtUnmapViewOfSectionFunction)(IN HANDLE ProcessHandle,
                                                       IN PVOID BaseAddress);

typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation = 0,
  SectionImageInformation
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef NTSTATUS(WINAPI* NtQuerySectionFunction)(
    IN HANDLE SectionHandle,
    IN SECTION_INFORMATION_CLASS SectionInformationClass,
    OUT PVOID SectionInformation,
    IN SIZE_T SectionInformationLength,
    OUT PSIZE_T ReturnLength OPTIONAL);

// -----------------------------------------------------------------------
// Process and Thread

// PCLIENT_ID not in winternl.h.
typedef CLIENT_ID* PCLIENT_ID;

typedef NTSTATUS(WINAPI* NtOpenThreadFunction)(OUT PHANDLE ThreadHandle,
                                               IN ACCESS_MASK DesiredAccess,
                                               IN POBJECT_ATTRIBUTES
                                                   ObjectAttributes,
                                               IN PCLIENT_ID ClientId);

typedef NTSTATUS(WINAPI* NtOpenProcessFunction)(OUT PHANDLE ProcessHandle,
                                                IN ACCESS_MASK DesiredAccess,
                                                IN POBJECT_ATTRIBUTES
                                                    ObjectAttributes,
                                                IN PCLIENT_ID ClientId);

// Provide ThreadImpersonationToken which is not in THREADINFOCLASS.
constexpr auto ThreadImpersonationToken = static_cast<THREADINFOCLASS>(5);

typedef NTSTATUS(WINAPI* NtSetInformationThreadFunction)(
    IN HANDLE ThreadHandle,
    IN THREADINFOCLASS ThreadInformationClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength);

// Partial definition only adding fields not in winternl.h, from
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa813706(v=vs.85).aspx
typedef struct _NT_PEB {
  BYTE InheritedAddressSpace;
  BYTE ReadImageFileExecOptions;
  BYTE BeingDebugged;
  BYTE SpareBool;
  PVOID Mutant;
  PVOID ImageBaseAddress;
  PVOID Ldr;
  PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
} NT_PEB, *PNT_PEB;

// Validate shared fields for NT_PEB:
static_assert(offsetof(NT_PEB, Ldr) == offsetof(PEB, Ldr));
static_assert(offsetof(NT_PEB, ProcessParameters) ==
              offsetof(PEB, ProcessParameters));

typedef NTSTATUS(WINAPI* NtQueryInformationProcessFunction)(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS(WINAPI* NtOpenThreadTokenFunction)(IN HANDLE ThreadHandle,
                                                    IN ACCESS_MASK
                                                        DesiredAccess,
                                                    IN BOOLEAN OpenAsSelf,
                                                    OUT PHANDLE TokenHandle);

typedef NTSTATUS(WINAPI* NtOpenThreadTokenExFunction)(IN HANDLE ThreadHandle,
                                                      IN ACCESS_MASK
                                                          DesiredAccess,
                                                      IN BOOLEAN OpenAsSelf,
                                                      IN ULONG HandleAttributes,
                                                      OUT PHANDLE TokenHandle);

typedef NTSTATUS(WINAPI* NtOpenProcessTokenFunction)(IN HANDLE ProcessHandle,
                                                     IN ACCESS_MASK
                                                         DesiredAccess,
                                                     OUT PHANDLE TokenHandle);

typedef NTSTATUS(WINAPI* NtOpenProcessTokenExFunction)(
    IN HANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    OUT PHANDLE TokenHandle);

// -----------------------------------------------------------------------
// Memory

// Don't really need this structure right now.
typedef PVOID PRTL_HEAP_PARAMETERS;

typedef PVOID(WINAPI* RtlCreateHeapFunction)(IN ULONG Flags,
                                             IN PVOID HeapBase OPTIONAL,
                                             IN SIZE_T ReserveSize OPTIONAL,
                                             IN SIZE_T CommitSize OPTIONAL,
                                             IN PVOID Lock OPTIONAL,
                                             IN PRTL_HEAP_PARAMETERS Parameters
                                                 OPTIONAL);

typedef PVOID(WINAPI* RtlDestroyHeapFunction)(IN PVOID HeapHandle);

typedef PVOID(WINAPI* RtlAllocateHeapFunction)(IN PVOID HeapHandle,
                                               IN ULONG Flags,
                                               IN SIZE_T Size);

typedef BOOLEAN(WINAPI* RtlFreeHeapFunction)(IN PVOID HeapHandle,
                                             IN ULONG Flags,
                                             IN PVOID HeapBase);

typedef NTSTATUS(WINAPI* NtAllocateVirtualMemoryFunction)(
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN OUT PSIZE_T RegionSize,
    IN ULONG AllocationType,
    IN ULONG Protect);

typedef NTSTATUS(WINAPI* NtFreeVirtualMemoryFunction)(IN HANDLE ProcessHandle,
                                                      IN OUT PVOID* BaseAddress,
                                                      IN OUT PSIZE_T RegionSize,
                                                      IN ULONG FreeType);

typedef enum _MEMORY_INFORMATION_CLASS {
  MemoryBasicInformation = 0,
  MemoryWorkingSetList,
  MemorySectionName,
  MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;

typedef struct _MEMORY_SECTION_NAME {  // Information Class 2 MemorySectionName.
  UNICODE_STRING SectionFileName;
} MEMORY_SECTION_NAME, *PMEMORY_SECTION_NAME;

typedef NTSTATUS(WINAPI* NtQueryVirtualMemoryFunction)(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress,
    IN MEMORY_INFORMATION_CLASS MemoryInformationClass,
    OUT PVOID MemoryInformation,
    IN SIZE_T MemoryInformationLength,
    OUT PSIZE_T ReturnLength OPTIONAL);

typedef NTSTATUS(WINAPI* NtProtectVirtualMemoryFunction)(
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN OUT PSIZE_T ProtectSize,
    IN ULONG NewProtect,
    OUT PULONG OldProtect);

// -----------------------------------------------------------------------
// Objects

// Add some field not in OBJECT_INFORMATION_CLASS.
constexpr auto ObjectNameInformation = static_cast<OBJECT_INFORMATION_CLASS>(1);

typedef enum _POOL_TYPE {
  NonPagedPool,
  PagedPool,
  NonPagedPoolMustSucceed,
  ReservedType,
  NonPagedPoolCacheAligned,
  PagedPoolCacheAligned,
  NonPagedPoolCacheAlignedMustS
} POOL_TYPE;

typedef struct _OBJECT_BASIC_INFORMATION {
  ULONG Attributes;
  ACCESS_MASK GrantedAccess;
  ULONG HandleCount;
  ULONG PointerCount;
  ULONG PagedPoolUsage;
  ULONG NonPagedPoolUsage;
  ULONG Reserved[3];
  ULONG NameInformationLength;
  ULONG TypeInformationLength;
  ULONG SecurityDescriptorLength;
  LARGE_INTEGER CreateTime;
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

typedef struct _OBJECT_TYPE_INFORMATION {
  UNICODE_STRING Name;
  ULONG TotalNumberOfObjects;
  ULONG TotalNumberOfHandles;
  ULONG TotalPagedPoolUsage;
  ULONG TotalNonPagedPoolUsage;
  ULONG TotalNamePoolUsage;
  ULONG TotalHandleTableUsage;
  ULONG HighWaterNumberOfObjects;
  ULONG HighWaterNumberOfHandles;
  ULONG HighWaterPagedPoolUsage;
  ULONG HighWaterNonPagedPoolUsage;
  ULONG HighWaterNamePoolUsage;
  ULONG HighWaterHandleTableUsage;
  ULONG InvalidAttributes;
  GENERIC_MAPPING GenericMapping;
  ULONG ValidAccess;
  BOOLEAN SecurityRequired;
  BOOLEAN MaintainHandleCount;
  USHORT MaintainTypeList;
  POOL_TYPE PoolType;
  ULONG PagedPoolUsage;
  ULONG NonPagedPoolUsage;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

typedef struct _OBJECT_NAME_INFORMATION {
  UNICODE_STRING ObjectName;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef NTSTATUS(WINAPI* NtQueryObjectFunction)(
    IN HANDLE Handle,
    IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
    OUT PVOID ObjectInformation OPTIONAL,
    IN ULONG ObjectInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS(WINAPI* NtDuplicateObjectFunction)(IN HANDLE SourceProcess,
                                                    IN HANDLE SourceHandle,
                                                    IN HANDLE TargetProcess,
                                                    OUT PHANDLE TargetHandle,
                                                    IN ACCESS_MASK
                                                        DesiredAccess,
                                                    IN ULONG Attributes,
                                                    IN ULONG Options);

typedef NTSTATUS(WINAPI* NtSignalAndWaitForSingleObjectFunction)(
    IN HANDLE HandleToSignal,
    IN HANDLE HandleToWait,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL);

typedef NTSTATUS(WINAPI* NtWaitForSingleObjectFunction)(
    IN HANDLE ObjectHandle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER TimeOut OPTIONAL);

// -----------------------------------------------------------------------
// Strings

typedef int(__cdecl* _strnicmpFunction)(IN const char* _Str1,
                                        IN const char* _Str2,
                                        IN size_t _MaxCount);

typedef size_t(__cdecl* strlenFunction)(IN const char* _Str);

typedef size_t(__cdecl* wcslenFunction)(IN const wchar_t* _Str);

typedef void*(__cdecl* memcpyFunction)(IN void* dest,
                                       IN const void* src,
                                       IN size_t count);

typedef NTSTATUS(WINAPI* RtlAnsiStringToUnicodeStringFunction)(
    IN OUT PUNICODE_STRING DestinationString,
    IN PANSI_STRING SourceString,
    IN BOOLEAN AllocateDestinationString);

typedef LONG(WINAPI* RtlCompareUnicodeStringFunction)(
    IN PCUNICODE_STRING String1,
    IN PCUNICODE_STRING String2,
    IN BOOLEAN CaseInSensitive);

typedef ULONG(WINAPI* RtlNtStatusToDosErrorFunction)(NTSTATUS status);

#endif  // SANDBOX_WIN_SRC_NT_INTERNALS_H__
