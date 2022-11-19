// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file holds definitions related to the ntdll API.

#ifndef SANDBOX_WIN_SRC_NT_INTERNALS_H__
#define SANDBOX_WIN_SRC_NT_INTERNALS_H__

#include <windows.h>

#include <stddef.h>

typedef LONG NTSTATUS;
#define NT_SUCCESS(st) (st >= 0)
#define NT_ERROR(st) ((((ULONG)(st)) >> 30) == 3)

// clang-format off
#define STATUS_SUCCESS                ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW        ((NTSTATUS)0x80000005L)
#define STATUS_UNSUCCESSFUL           ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED        ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_INFO_CLASS     ((NTSTATUS)0xC0000003L)
#define STATUS_INFO_LENGTH_MISMATCH   ((NTSTATUS)0xC0000004L)
#ifndef STATUS_INVALID_PARAMETER
// It is now defined in Windows 2008 SDK.
#define STATUS_INVALID_PARAMETER      ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_CONFLICTING_ADDRESSES  ((NTSTATUS)0xC0000018L)
#define STATUS_ACCESS_DENIED          ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL       ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_NOT_FOUND  ((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_NAME_COLLISION  ((NTSTATUS)0xC0000035L)
#define STATUS_PROCEDURE_NOT_FOUND    ((NTSTATUS)0xC000007AL)
#define STATUS_INVALID_IMAGE_FORMAT   ((NTSTATUS)0xC000007BL)
#define STATUS_NO_TOKEN               ((NTSTATUS)0xC000007CL)
#define STATUS_NOT_SUPPORTED          ((NTSTATUS)0xC00000BBL)
#define STATUS_INVALID_IMAGE_HASH     ((NTSTATUS)0xC0000428L)

#define NTSTATUS_INVALID_PARAMETER    ((NTSTATUS)0xC000000DL)
#define NTSTATUS_NO_MEMORY            ((NTSTATUS)0xC0000017L)
// clang-format on

#define CURRENT_PROCESS ((HANDLE)-1)
#define CURRENT_THREAD ((HANDLE)-2)
#define NtCurrentProcess CURRENT_PROCESS

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

typedef struct _STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} STRING;
typedef STRING* PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef CONST PSTRING PCANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;
typedef CONST STRING* PCOEM_STRING;

#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_OPENIF 0x00000080L

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES* POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p, n, a, r, s) \
  {                                               \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES);      \
    (p)->RootDirectory = r;                       \
    (p)->Attributes = a;                          \
    (p)->ObjectName = n;                          \
    (p)->SecurityDescriptor = s;                  \
    (p)->SecurityQualityOfService = nullptr;      \
  }

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  };
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

// -----------------------------------------------------------------------
// File IO

// Create disposition values.

#define FILE_SUPERSEDE                          0x00000000
#define FILE_OPEN                               0x00000001
#define FILE_CREATE                             0x00000002
#define FILE_OPEN_IF                            0x00000003
#define FILE_OVERWRITE                          0x00000004
#define FILE_OVERWRITE_IF                       0x00000005
#define FILE_MAXIMUM_DISPOSITION                0x00000005

// Create/open option flags.

#define FILE_DIRECTORY_FILE                     0x00000001
#define FILE_WRITE_THROUGH                      0x00000002
#define FILE_SEQUENTIAL_ONLY                    0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING          0x00000008

#define FILE_SYNCHRONOUS_IO_ALERT               0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT            0x00000020
#define FILE_NON_DIRECTORY_FILE                 0x00000040
#define FILE_CREATE_TREE_CONNECTION             0x00000080

#define FILE_COMPLETE_IF_OPLOCKED               0x00000100
#define FILE_NO_EA_KNOWLEDGE                    0x00000200
#define FILE_OPEN_REMOTE_INSTANCE               0x00000400
#define FILE_RANDOM_ACCESS                      0x00000800

#define FILE_DELETE_ON_CLOSE                    0x00001000
#define FILE_OPEN_BY_FILE_ID                    0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT             0x00004000
#define FILE_NO_COMPRESSION                     0x00008000

#define FILE_RESERVE_OPFILTER                   0x00100000
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#define FILE_OPEN_NO_RECALL                     0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY          0x00800000

// Create/open result values. These are the disposition values returned on the
// io status information.
#define FILE_SUPERSEDED                         0x00000000
#define FILE_OPENED                             0x00000001
#define FILE_CREATED                            0x00000002
#define FILE_OVERWRITTEN                        0x00000003
#define FILE_EXISTS                             0x00000004
#define FILE_DOES_NOT_EXIST                     0x00000005

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

typedef enum _FILE_INFORMATION_CLASS {
  FileRenameInformation = 10
} FILE_INFORMATION_CLASS,
    *PFILE_INFORMATION_CLASS;

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

typedef struct _CLIENT_ID {
  PVOID UniqueProcess;
  PVOID UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

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

typedef enum _NT_THREAD_INFORMATION_CLASS {
  ThreadBasicInformation,
  ThreadTimes,
  ThreadPriority,
  ThreadBasePriority,
  ThreadAffinityMask,
  ThreadImpersonationToken,
  ThreadDescriptorTableEntry,
  ThreadEnableAlignmentFaultFixup,
  ThreadEventPair,
  ThreadQuerySetWin32StartAddress,
  ThreadZeroTlsCell,
  ThreadPerformanceCount,
  ThreadAmILastThread,
  ThreadIdealProcessor,
  ThreadPriorityBoost,
  ThreadSetTlsArrayAddress,
  ThreadIsIoPending,
  ThreadHideFromDebugger
} NT_THREAD_INFORMATION_CLASS,
    *PNT_THREAD_INFORMATION_CLASS;

typedef NTSTATUS(WINAPI* NtSetInformationThreadFunction)(
    IN HANDLE ThreadHandle,
    IN NT_THREAD_INFORMATION_CLASS ThreadInformationClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength);

// Partial definition only:
typedef enum _PROCESSINFOCLASS {
  ProcessBasicInformation = 0,
  ProcessExecuteFlags = 0x22,
  ProcessHandleTable = 0x3A
} PROCESSINFOCLASS;

// For the structure documentation, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa813741(v=vs.85).aspx
typedef struct _RTL_USER_PROCESS_PARAMETERS {
  BYTE Reserved1[16];
  PVOID Reserved2[10];
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

// Partial definition only, from
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa813706(v=vs.85).aspx
typedef struct _PEB {
  BYTE InheritedAddressSpace;
  BYTE ReadImageFileExecOptions;
  BYTE BeingDebugged;
  BYTE SpareBool;
  PVOID Mutant;
  PVOID ImageBaseAddress;
  PVOID Ldr;
  PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
} PEB, *PPEB;

typedef LONG KPRIORITY;

typedef struct _PROCESS_BASIC_INFORMATION {
  union {
    NTSTATUS ExitStatus;
    PVOID padding_for_x64_0;
  };
  PPEB PebBaseAddress;
  KAFFINITY AffinityMask;
  union {
    KPRIORITY BasePriority;
    PVOID padding_for_x64_1;
  };
  union {
    DWORD UniqueProcessId;
    PVOID padding_for_x64_2;
  };
  union {
    DWORD InheritedFromUniqueProcessId;
    PVOID padding_for_x64_3;
  };
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

typedef NTSTATUS(WINAPI* NtQueryInformationProcessFunction)(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS(WINAPI* NtSetInformationProcessFunction)(
    HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    IN PVOID ProcessInformation,
    IN ULONG ProcessInformationLength);

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

typedef NTSTATUS(WINAPI* NtQueryInformationTokenFunction)(
    IN HANDLE TokenHandle,
    IN TOKEN_INFORMATION_CLASS TokenInformationClass,
    OUT PVOID TokenInformation,
    IN ULONG TokenInformationLength,
    OUT PULONG ReturnLength);

typedef NTSTATUS(WINAPI* RtlCreateUserThreadFunction)(
    IN HANDLE Process,
    IN PSECURITY_DESCRIPTOR ThreadSecurityDescriptor,
    IN BOOLEAN CreateSuspended,
    IN ULONG ZeroBits,
    IN SIZE_T MaximumStackSize,
    IN SIZE_T CommittedStackSize,
    IN LPTHREAD_START_ROUTINE StartAddress,
    IN PVOID Parameter,
    OUT PHANDLE Thread,
    OUT PCLIENT_ID ClientId);

typedef NTSTATUS(WINAPI* RtlConvertSidToUnicodeStringFunction)(
    OUT PUNICODE_STRING UnicodeString,
    IN PSID Sid,
    IN BOOLEAN AllocateDestinationString);

typedef VOID(WINAPI* RtlFreeUnicodeStringFunction)(
    IN OUT PUNICODE_STRING UnicodeString);

// -----------------------------------------------------------------------
// Registry

typedef enum _KEY_INFORMATION_CLASS {
  KeyBasicInformation = 0,
  KeyFullInformation = 2
} KEY_INFORMATION_CLASS,
    *PKEY_INFORMATION_CLASS;

typedef struct _KEY_BASIC_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG NameLength;
  WCHAR Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;

typedef struct _KEY_FULL_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG ClassOffset;
  ULONG ClassLength;
  ULONG SubKeys;
  ULONG MaxNameLen;
  ULONG MaxClassLen;
  ULONG Values;
  ULONG MaxValueNameLen;
  ULONG MaxValueDataLen;
  WCHAR Class[1];
} KEY_FULL_INFORMATION, *PKEY_FULL_INFORMATION;

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

typedef NTSTATUS(WINAPI* NtCreateKeyFunction)(OUT PHANDLE KeyHandle,
                                              IN ACCESS_MASK DesiredAccess,
                                              IN POBJECT_ATTRIBUTES
                                                  ObjectAttributes,
                                              IN ULONG TitleIndex,
                                              IN PUNICODE_STRING Class OPTIONAL,
                                              IN ULONG CreateOptions,
                                              OUT PULONG Disposition OPTIONAL);

typedef NTSTATUS(WINAPI* NtOpenKeyFunction)(OUT PHANDLE KeyHandle,
                                            IN ACCESS_MASK DesiredAccess,
                                            IN POBJECT_ATTRIBUTES
                                                ObjectAttributes);

typedef NTSTATUS(WINAPI* NtOpenKeyExFunction)(OUT PHANDLE KeyHandle,
                                              IN ACCESS_MASK DesiredAccess,
                                              IN POBJECT_ATTRIBUTES
                                                  ObjectAttributes,
                                              IN DWORD open_options);

typedef NTSTATUS(WINAPI* NtDeleteKeyFunction)(IN HANDLE KeyHandle);

typedef NTSTATUS(WINAPI* RtlFormatCurrentUserKeyPathFunction)(
    OUT PUNICODE_STRING RegistryPath);

typedef NTSTATUS(WINAPI* NtQueryKeyFunction)(IN HANDLE KeyHandle,
                                             IN KEY_INFORMATION_CLASS
                                                 KeyInformationClass,
                                             OUT PVOID KeyInformation,
                                             IN ULONG Length,
                                             OUT PULONG ResultLength);

typedef NTSTATUS(WINAPI* NtEnumerateKeyFunction)(IN HANDLE KeyHandle,
                                                 IN ULONG Index,
                                                 IN KEY_INFORMATION_CLASS
                                                     KeyInformationClass,
                                                 OUT PVOID KeyInformation,
                                                 IN ULONG Length,
                                                 OUT PULONG ResultLength);

typedef NTSTATUS(WINAPI* NtQueryValueKeyFunction)(IN HANDLE KeyHandle,
                                                  IN PUNICODE_STRING ValueName,
                                                  IN KEY_VALUE_INFORMATION_CLASS
                                                      KeyValueInformationClass,
                                                  OUT PVOID KeyValueInformation,
                                                  IN ULONG Length,
                                                  OUT PULONG ResultLength);

typedef NTSTATUS(WINAPI* NtSetValueKeyFunction)(IN HANDLE KeyHandle,
                                                IN PUNICODE_STRING ValueName,
                                                IN ULONG TitleIndex OPTIONAL,
                                                IN ULONG Type,
                                                IN PVOID Data,
                                                IN ULONG DataSize);

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

typedef struct _MEMORY_SECTION_NAME {  // Information Class 2
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

typedef enum _OBJECT_INFORMATION_CLASS {
  ObjectBasicInformation,
  ObjectNameInformation,
  ObjectTypeInformation,
  ObjectAllInformation,
  ObjectDataInformation
} OBJECT_INFORMATION_CLASS,
    *POBJECT_INFORMATION_CLASS;

typedef struct _OBJDIR_INFORMATION {
  UNICODE_STRING ObjectName;
  UNICODE_STRING ObjectTypeName;
  BYTE Data[1];
} OBJDIR_INFORMATION;

typedef struct _PUBLIC_OBJECT_BASIC_INFORMATION {
  ULONG Attributes;
  ACCESS_MASK GrantedAccess;
  ULONG HandleCount;
  ULONG PointerCount;
  ULONG Reserved[10];  // reserved for internal use
} PUBLIC_OBJECT_BASIC_INFORMATION, *PPUBLIC_OBJECT_BASIC_INFORMATION;

typedef struct __PUBLIC_OBJECT_TYPE_INFORMATION {
  UNICODE_STRING TypeName;
  ULONG Reserved[22];  // reserved for internal use
} PUBLIC_OBJECT_TYPE_INFORMATION, *PPUBLIC_OBJECT_TYPE_INFORMATION;

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

typedef enum _SYSTEM_INFORMATION_CLASS {
  SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_HANDLE_INFORMATION {
  USHORT ProcessId;
  USHORT CreatorBackTraceIndex;
  UCHAR ObjectTypeNumber;
  UCHAR Flags;
  USHORT Handle;
  PVOID Object;
  ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
  ULONG NumberOfHandles;
  SYSTEM_HANDLE_INFORMATION Information[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

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

typedef NTSTATUS(WINAPI* NtQuerySystemInformation)(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength);

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

typedef VOID(WINAPI* RtlInitUnicodeStringFunction)(IN OUT PUNICODE_STRING
                                                       DestinationString,
                                                   IN PCWSTR SourceString);

typedef ULONG(WINAPI* RtlNtStatusToDosErrorFunction)(NTSTATUS status);

typedef NTSTATUS(WINAPI* NtSetInformationProcess)(IN HANDLE process_handle,
                                                  IN ULONG info_class,
                                                  IN PVOID process_information,
                                                  IN ULONG information_length);

struct PROCESS_ACCESS_TOKEN {
  HANDLE token;
  HANDLE thread;
};

const unsigned int NtProcessInformationAccessToken = 9;

typedef NTSTATUS(WINAPI* RtlDeriveCapabilitySidsFromNameFunction)(
    PCUNICODE_STRING SourceString,
    PSID CapabilityGroupSid,
    PSID CapabilitySid);

#endif  // SANDBOX_WIN_SRC_NT_INTERNALS_H__
