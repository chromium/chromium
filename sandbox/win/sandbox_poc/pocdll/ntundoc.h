// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SANDBOX_POC_POCDLL_NTUNDOC_H_
#define SANDBOX_WIN_SANDBOX_POC_POCDLL_NTUNDOC_H_

#define NTSTATUS ULONG
#define STATUS_SUCCESS 0x00000000
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004
#define STATUS_ACCESS_DENIED 0xC0000022
#define STATUS_BUFFER_OVERFLOW 0x80000005

typedef struct _LSA_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING;

typedef struct _OBJDIR_INFORMATION {
  UNICODE_STRING ObjectName;
  UNICODE_STRING ObjectTypeName;
  BYTE Data[1];
} OBJDIR_INFORMATION;

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  UNICODE_STRING *ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;

typedef struct _PUBLIC_OBJECT_BASIC_INFORMATION {
  ULONG Attributes;
  ACCESS_MASK GrantedAccess;
  ULONG HandleCount;
  ULONG PointerCount;
  ULONG Reserved[10];    // reserved for internal use
 } PUBLIC_OBJECT_BASIC_INFORMATION, *PPUBLIC_OBJECT_BASIC_INFORMATION;

typedef struct __PUBLIC_OBJECT_TYPE_INFORMATION {
  UNICODE_STRING TypeName;
  ULONG Reserved [22];    // reserved for internal use
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
  UNICODE_STRING          ObjectName;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef enum _OBJECT_INFORMATION_CLASS {
  ObjectBasicInformation,
  ObjectNameInformation,
  ObjectTypeInformation,
  ObjectAllInformation,
  ObjectDataInformation
} OBJECT_INFORMATION_CLASS, *POBJECT_INFORMATION_CLASS;

typedef struct _FILE_NAME_INFORMATION {
 ULONG FileNameLength;
 WCHAR FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

typedef enum _FILE_INFORMATION_CLASS {
 // end_wdm
  FileDirectoryInformation       = 1,
  FileFullDirectoryInformation, // 2
  FileBothDirectoryInformation, // 3
  FileBasicInformation,         // 4  wdm
  FileStandardInformation,      // 5  wdm
  FileInternalInformation,      // 6
  FileEaInformation,            // 7
  FileAccessInformation,        // 8
  FileNameInformation,          // 9
  FileRenameInformation,        // 10
  FileLinkInformation,          // 11
  FileNamesInformation,         // 12
  FileDispositionInformation,   // 13
  FilePositionInformation,      // 14 wdm
  FileFullEaInformation,        // 15
  FileModeInformation,          // 16
  FileAlignmentInformation,     // 17
  FileAllInformation,           // 18
  FileAllocationInformation,    // 19
  FileEndOfFileInformation,     // 20 wdm
  FileAlternateNameInformation, // 21
  FileStreamInformation,        // 22
  FilePipeInformation,          // 23
  FilePipeLocalInformation,     // 24
  FilePipeRemoteInformation,    // 25
  FileMailslotQueryInformation, // 26
  FileMailslotSetInformation,   // 27
  FileCompressionInformation,   // 28
  FileObjectIdInformation,      // 29
  FileCompletionInformation,    // 30
  FileMoveClusterInformation,   // 31
  FileQuotaInformation,         // 32
  FileReparsePointInformation,  // 33
  FileNetworkOpenInformation,   // 34
  FileAttributeTagInformation,  // 35
  FileTrackingInformation,      // 36
  FileMaximumInformation
  // begin_wdm
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef enum _SYSTEM_INFORMATION_CLASS {
  SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASS;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES ); \
    (p)->RootDirectory = r; \
    (p)->Attributes = a; \
    (p)->ObjectName = n; \
    (p)->SecurityDescriptor = s; \
    (p)->SecurityQualityOfService = NULL; \
}

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

#define POBJECT_ATTRIBUTES OBJECT_ATTRIBUTES*

typedef NTSTATUS (WINAPI* NTQUERYDIRECTORYOBJECT)(
  HANDLE,
  OBJDIR_INFORMATION*,
  DWORD,
  DWORD,
  DWORD,
  DWORD*,
  DWORD*);

typedef NTSTATUS (WINAPI* NTOPENDIRECTORYOBJECT)(
  HANDLE *,
  DWORD,
  OBJECT_ATTRIBUTES* );

typedef NTSTATUS (WINAPI* NTGENERICOPEN) (
  OUT PHANDLE EventHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENEVENT)(
  OUT PHANDLE EventHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENJOBOBJECT)(
  OUT PHANDLE JobHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENKEYEDEVENT)(
  OUT PHANDLE KeyedEventHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENMUTANT)(
  OUT PHANDLE MutantHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENSECTION)(
  OUT PHANDLE SectionHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENSEMAPHORE)(
  OUT PHANDLE SemaphoreHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENSYMBOLICLINKOBJECT)(
  OUT PHANDLE SymbolicLinkHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENTIMER)(
  OUT PHANDLE TimerHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI* NTOPENFILE)(
  HANDLE *,
  DWORD,
  OBJECT_ATTRIBUTES *,
  IO_STATUS_BLOCK *,
  DWORD,
  DWORD);

typedef NTSTATUS (WINAPI* NTQUERYINFORMATIONFILE)(
  HANDLE,
  PIO_STATUS_BLOCK,
  PVOID,
  ULONG,
  FILE_INFORMATION_CLASS);

typedef NTSTATUS (WINAPI* NTQUERYSYSTEMINFORMATION)(
  SYSTEM_INFORMATION_CLASS SystemInformationClass,
  PVOID SystemInformation,
  ULONG SystemInformationLength,
  PULONG ReturnLength);

typedef NTSTATUS (WINAPI* NTQUERYOBJECT)(
  HANDLE Handle,
  OBJECT_INFORMATION_CLASS ObjectInformationClass,
  PVOID ObjectInformation,
  ULONG ObjectInformationLength,
  PULONG ReturnLength);

typedef NTSTATUS (WINAPI* NTCLOSE) (HANDLE);

#define DIRECTORY_QUERY 0x0001
#define DIRECTORY_TRAVERSE 0x0002
#define DIRECTORY_CREATE_OBJECT 0x0004
#define DIRECTORY_CREATE_SUBDIRECTORY 0x0008
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)

#endif  // SANDBOX_WIN_SANDBOX_POC_POCDLL_NTUNDOC_H_
