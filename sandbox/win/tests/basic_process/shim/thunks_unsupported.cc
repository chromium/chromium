// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Logged pass-through thunks for APIs outside the basic-process allowlist.
// Keep generated forwarders sorted by DLL name, then by API name within each
// DLL section.

#include <windows.h>

#include <aclapi.h>
#include <appmodel.h>
#include <evntprov.h>
#include <evntrace.h>
#include <mmeapi.h>
#include <oleauto.h>
#include <psapi.h>
#include <roapi.h>
#include <sddl.h>
#include <timeapi.h>
#include <tlhelp32.h>
#include <werapi.h>
#include <winstring.h>
#include <wtypes.h>

#include "sandbox/win/tests/basic_process/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/bcryptprimitives_declarations.h"
#include "sandbox/win/tests/basic_process/shim/dwrite_declarations.h"
#include "sandbox/win/tests/basic_process/shim/ntdll_declarations.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

extern "C" {

// --- advapi32 passthroughs ---

APIFW_FORWARD_ADVAPI32(ConvertSidToStringSidA, (PSID, Sid), (LPSTR*, StringSid))
APIFW_FORWARD_ADVAPI32(ConvertSidToStringSidW,
                       (PSID, Sid),
                       (LPWSTR*, StringSid))
APIFW_FORWARD_ADVAPI32(ConvertStringSidToSidW,
                       (LPCWSTR, StringSid),
                       (PSID*, Sid))
APIFW_FORWARD_ADVAPI32(EqualSid, (PSID, pSid1), (PSID, pSid2))
APIFW_FORWARD_ADVAPI32(EventRegister,
                       (LPCGUID, ProviderId),
                       (PENABLECALLBACK, EnableCallback),
                       (PVOID, CallbackContext),
                       (PREGHANDLE, RegHandle))
APIFW_FORWARD_ADVAPI32(EventSetInformation,
                       (REGHANDLE, RegHandle),
                       (EVENT_INFO_CLASS, InformationClass),
                       (PVOID, EventInformation),
                       (ULONG, InformationLength))
APIFW_FORWARD_ADVAPI32(FreeSid, (PSID, pSid))
APIFW_FORWARD_ADVAPI32(GetAce,
                       (PACL, pAcl),
                       (DWORD, dwAceIndex),
                       (LPVOID*, pAce))
APIFW_FORWARD_ADVAPI32(GetLengthSid, (PSID, pSid))
APIFW_FORWARD_ADVAPI32(GetNamedSecurityInfoW,
                       (LPCWSTR, pObjectName),
                       (SE_OBJECT_TYPE, ObjectType),
                       (SECURITY_INFORMATION, SecurityInfo),
                       (PSID*, ppsidOwner),
                       (PSID*, ppsidGroup),
                       (PACL*, ppDacl),
                       (PACL*, ppSacl),
                       (PSECURITY_DESCRIPTOR*, ppSecurityDescriptor))
APIFW_FORWARD_ADVAPI32(GetSecurityDescriptorControl,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (PSECURITY_DESCRIPTOR_CONTROL, pControl),
                       (LPDWORD, lpdwRevision))
APIFW_FORWARD_ADVAPI32(GetSecurityDescriptorDacl,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (LPBOOL, lpbDaclPresent),
                       (PACL*, pDacl),
                       (LPBOOL, lpbDaclDefaulted))
APIFW_FORWARD_ADVAPI32(GetSecurityDescriptorGroup,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (PSID*, pGroup),
                       (LPBOOL, lpbGroupDefaulted))
APIFW_FORWARD_ADVAPI32(GetSecurityDescriptorOwner,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (PSID*, pOwner),
                       (LPBOOL, lpbOwnerDefaulted))
APIFW_FORWARD_ADVAPI32(GetSecurityDescriptorSacl,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (LPBOOL, lpbSaclPresent),
                       (PACL*, pSacl),
                       (LPBOOL, lpbSaclDefaulted))
APIFW_FORWARD_ADVAPI32(GetSecurityInfo,
                       (HANDLE, handle),
                       (SE_OBJECT_TYPE, ObjectType),
                       (SECURITY_INFORMATION, SecurityInfo),
                       (PSID*, ppsidOwner),
                       (PSID*, ppsidGroup),
                       (PACL*, ppDacl),
                       (PACL*, ppSacl),
                       (PSECURITY_DESCRIPTOR*, ppSecurityDescriptor))
APIFW_FORWARD_ADVAPI32(GetSidSubAuthority, (PSID, pSid), (DWORD, nSubAuthority))
APIFW_FORWARD_ADVAPI32(GetSidSubAuthorityCount, (PSID, pSid))
APIFW_FORWARD_ADVAPI32(GetTokenInformation,
                       (HANDLE, TokenHandle),
                       (TOKEN_INFORMATION_CLASS, TokenInformationClass),
                       (LPVOID, TokenInformation),
                       (DWORD, TokenInformationLength),
                       (PDWORD, ReturnLength))
APIFW_FORWARD_ADVAPI32(GetTraceEnableFlags, (TRACEHANDLE, TraceHandle))
APIFW_FORWARD_ADVAPI32(GetTraceEnableLevel, (TRACEHANDLE, TraceHandle))
APIFW_FORWARD_ADVAPI32(GetTraceLoggerHandle, (PVOID, Buffer))
APIFW_FORWARD_ADVAPI32(InitializeAcl,
                       (PACL, pAcl),
                       (DWORD, nAclLength),
                       (DWORD, dwAclRevision))
APIFW_FORWARD_ADVAPI32(InitializeSecurityDescriptor,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (DWORD, dwRevision))
APIFW_FORWARD_ADVAPI32(IsValidAcl, (PACL, pAcl))
APIFW_FORWARD_ADVAPI32(IsValidSecurityDescriptor,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor))
APIFW_FORWARD_ADVAPI32(IsValidSid, (PSID, pSid))
APIFW_FORWARD_ADVAPI32(RegCreateKeyExW,
                       (HKEY, hKey),
                       (LPCWSTR, lpSubKey),
                       (DWORD, Reserved),
                       (LPWSTR, lpClass),
                       (DWORD, dwOptions),
                       (REGSAM, samDesired),
                       (const LPSECURITY_ATTRIBUTES, lpSecurityAttributes),
                       (PHKEY, phkResult),
                       (LPDWORD, lpdwDisposition))
APIFW_FORWARD_ADVAPI32(RegDeleteKeyExW,
                       (HKEY, hKey),
                       (LPCWSTR, lpSubKey),
                       (REGSAM, samDesired),
                       (DWORD, Reserved))
APIFW_FORWARD_ADVAPI32(RegDeleteValueW, (HKEY, hKey), (LPCWSTR, lpValueName))
APIFW_FORWARD_ADVAPI32(RegDisablePredefinedCache)
APIFW_FORWARD_ADVAPI32(RegEnumKeyExW,
                       (HKEY, hKey),
                       (DWORD, dwIndex),
                       (LPWSTR, lpName),
                       (LPDWORD, lpcchName),
                       (LPDWORD, lpReserved),
                       (LPWSTR, lpClass),
                       (LPDWORD, lpcchClass),
                       (PFILETIME, lpftLastWriteTime))
APIFW_FORWARD_ADVAPI32(RegEnumValueW,
                       (HKEY, hKey),
                       (DWORD, dwIndex),
                       (LPWSTR, lpValueName),
                       (LPDWORD, lpcchValueName),
                       (LPDWORD, lpReserved),
                       (LPDWORD, lpType),
                       (LPBYTE, lpData),
                       (LPDWORD, lpcbData))
APIFW_FORWARD_ADVAPI32(RegGetValueW,
                       (HKEY, hkey),
                       (LPCWSTR, lpSubKey),
                       (LPCWSTR, lpValue),
                       (DWORD, dwFlags),
                       (LPDWORD, pdwType),
                       (PVOID, pvData),
                       (LPDWORD, pcbData))
APIFW_FORWARD_ADVAPI32(RegOverridePredefKey, (HKEY, hKey), (HKEY, hNewHKey))
APIFW_FORWARD_ADVAPI32(RegQueryInfoKeyW,
                       (HKEY, hKey),
                       (LPWSTR, lpClass),
                       (LPDWORD, lpcchClass),
                       (LPDWORD, lpReserved),
                       (LPDWORD, lpcSubKeys),
                       (LPDWORD, lpcbMaxSubKeyLen),
                       (LPDWORD, lpcbMaxClassLen),
                       (LPDWORD, lpcValues),
                       (LPDWORD, lpcbMaxValueNameLen),
                       (LPDWORD, lpcbMaxValueLen),
                       (LPDWORD, lpcbSecurityDescriptor),
                       (PFILETIME, lpftLastWriteTime))
APIFW_FORWARD_ADVAPI32(RegQueryValueExA,
                       (HKEY, hKey),
                       (LPCSTR, lpValueName),
                       (LPDWORD, lpReserved),
                       (LPDWORD, lpType),
                       (LPBYTE, lpData),
                       (LPDWORD, lpcbData))
APIFW_FORWARD_ADVAPI32(RegSetValueExW,
                       (HKEY, hKey),
                       (LPCWSTR, lpValueName),
                       (DWORD, Reserved),
                       (DWORD, dwType),
                       (const BYTE*, lpData),
                       (DWORD, cbData))
APIFW_FORWARD_ADVAPI32(RegisterTraceGuidsW,
                       (WMIDPREQUEST, RequestAddress),
                       (PVOID, RequestContext),
                       (LPCGUID, ControlGuid),
                       (ULONG, GuidCount),
                       (PTRACE_GUID_REGISTRATION, TraceGuidReg),
                       (LPCWSTR, MofImagePath),
                       (LPCWSTR, MofResourceName),
                       (PTRACEHANDLE, RegistrationHandle))
APIFW_FORWARD_ADVAPI32(UnregisterTraceGuids, (TRACEHANDLE, RegistrationHandle))
APIFW_FORWARD_ADVAPI32(SetSecurityDescriptorDacl,
                       (PSECURITY_DESCRIPTOR, pSecurityDescriptor),
                       (BOOL, bDaclPresent),
                       (PACL, pDacl),
                       (BOOL, bDaclDefaulted))
APIFW_FORWARD_ADVAPI32(SetSecurityInfo,
                       (HANDLE, handle),
                       (SE_OBJECT_TYPE, ObjectType),
                       (SECURITY_INFORMATION, SecurityInfo),
                       (PSID, psidOwner),
                       (PSID, psidGroup),
                       (PACL, pDacl),
                       (PACL, pSacl))
APIFW_FORWARD_ADVAPI32(TraceEvent,
                       (TRACEHANDLE, TraceHandle),
                       (PEVENT_TRACE_HEADER, EventTrace))

// --- bcryptprimitives passthroughs ---

APIFW_FORWARD_BCRYPTPRIMITIVES(ProcessPrng, (PBYTE, pbData), (SIZE_T, cbData))

// --- combase passthroughs ---

APIFW_FORWARD_COMBASE(RoActivateInstance,
                      (HSTRING, activatableClassId),
                      (IInspectable**, instance))
APIFW_FORWARD_COMBASE(RoGetActivationFactory,
                      (HSTRING, activatableClassId),
                      (REFIID, iid),
                      (void**, factory))
APIFW_FORWARD_COMBASE(WindowsCreateString,
                      (PCNZWCH, sourceString),
                      (UINT32, length),
                      (HSTRING*, string))
APIFW_FORWARD_COMBASE(WindowsCreateStringReference,
                      (PCWSTR, sourceString),
                      (UINT32, length),
                      (HSTRING_HEADER*, hstringHeader),
                      (HSTRING*, string))
APIFW_FORWARD_COMBASE(WindowsDeleteString, (HSTRING, string))

// --- dwrite passthroughs ---

APIFW_FORWARD_DWRITE(DWriteCreateFactory,
                     (DWRITE_FACTORY_TYPE, factoryType),
                     (REFIID, iid),
                     (IUnknown**, factory))
APIFW_FORWARD_DWRITECORE(DWriteCoreCreateFactory,
                         (DWRITE_FACTORY_TYPE, factoryType),
                         (REFIID, iid),
                         (IUnknown**, factory))

// --- kernel32 passthroughs ---

APIFW_FORWARD_KERNEL32(AttachConsole, (DWORD, dwProcessId))
APIFW_FORWARD_KERNEL32(CancelIo, (HANDLE, hFile))
APIFW_FORWARD_KERNEL32(CompareStringEx,
                       (LPCWSTR, lpLocaleName),
                       (DWORD, dwCmpFlags),
                       (LPCWCH, lpString1),
                       (int, cchCount1),
                       (LPCWCH, lpString2),
                       (int, cchCount2),
                       (LPNLSVERSIONINFO, lpVersionInformation),
                       (LPVOID, lpReserved),
                       (LPARAM, lParam))
APIFW_FORWARD_KERNEL32(ConnectNamedPipe,
                       (HANDLE, hNamedPipe),
                       (LPOVERLAPPED, lpOverlapped))
APIFW_FORWARD_KERNEL32(CreateEventW,
                       (LPSECURITY_ATTRIBUTES, lpEventAttributes),
                       (BOOL, bManualReset),
                       (BOOL, bInitialState),
                       (LPCWSTR, lpName))
APIFW_FORWARD_KERNEL32(CreateFileMappingW,
                       (HANDLE, hFile),
                       (LPSECURITY_ATTRIBUTES, lpFileMappingAttributes),
                       (DWORD, flProtect),
                       (DWORD, dwMaximumSizeHigh),
                       (DWORD, dwMaximumSizeLow),
                       (LPCWSTR, lpName))
APIFW_FORWARD_KERNEL32(CreateIoCompletionPort,
                       (HANDLE, FileHandle),
                       (HANDLE, ExistingCompletionPort),
                       (ULONG_PTR, CompletionKey),
                       (DWORD, NumberOfConcurrentThreads))
APIFW_FORWARD_KERNEL32(CreateNamedPipeW,
                       (LPCWSTR, lpName),
                       (DWORD, dwOpenMode),
                       (DWORD, dwPipeMode),
                       (DWORD, nMaxInstances),
                       (DWORD, nOutBufferSize),
                       (DWORD, nInBufferSize),
                       (DWORD, nDefaultTimeOut),
                       (LPSECURITY_ATTRIBUTES, lpSecurityAttributes))
APIFW_FORWARD_KERNEL32(CreateSemaphoreA,
                       (LPSECURITY_ATTRIBUTES, lpSemaphoreAttributes),
                       (LONG, lInitialCount),
                       (LONG, lMaximumCount),
                       (LPCSTR, lpName))
APIFW_FORWARD_KERNEL32(CreateSymbolicLinkW,
                       (LPCWSTR, lpSymlinkFileName),
                       (LPCWSTR, lpTargetFileName),
                       (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(CreateToolhelp32Snapshot,
                       (DWORD, dwFlags),
                       (DWORD, th32ProcessID))
APIFW_FORWARD_KERNEL32(DiscardVirtualMemory,
                       (PVOID, VirtualAddress),
                       (SIZE_T, Size))
APIFW_FORWARD_KERNEL32(DuplicateHandle,
                       (HANDLE, hSourceProcessHandle),
                       (HANDLE, hSourceHandle),
                       (HANDLE, hTargetProcessHandle),
                       (LPHANDLE, lpTargetHandle),
                       (DWORD, dwDesiredAccess),
                       (BOOL, bInheritHandle),
                       (DWORD, dwOptions))
APIFW_FORWARD_KERNEL32(FindClose, (HANDLE, hFindFile))
APIFW_FORWARD_KERNEL32(FindFirstFileExW,
                       (LPCWSTR, lpFileName),
                       (FINDEX_INFO_LEVELS, fInfoLevelId),
                       (LPVOID, lpFindFileData),
                       (FINDEX_SEARCH_OPS, fSearchOp),
                       (LPVOID, lpSearchFilter),
                       (DWORD, dwAdditionalFlags))
APIFW_FORWARD_KERNEL32(FindFirstFileW,
                       (LPCWSTR, lpFileName),
                       (LPWIN32_FIND_DATAW, lpFindFileData))
APIFW_FORWARD_KERNEL32(FindNextFileW,
                       (HANDLE, hFindFile),
                       (LPWIN32_FIND_DATAW, lpFindFileData))
APIFW_FORWARD_KERNEL32(FindResourceW,
                       (HMODULE, hModule),
                       (LPCWSTR, lpName),
                       (LPCWSTR, lpType))

APIFW_FORWARD_KERNEL32(AreFileApisANSI)
APIFW_FORWARD_KERNEL32(CompareStringW,
                       (LCID, Locale),
                       (DWORD, dwCmpFlags),
                       (LPCWSTR, lpString1),
                       (int, cchCount1),
                       (LPCWSTR, lpString2),
                       (int, cchCount2))
APIFW_FORWARD_KERNEL32(EnumSystemLocalesEx,
                       (LOCALE_ENUMPROCEX, lpLocaleEnumProcEx),
                       (DWORD, dwFlags),
                       (LPARAM, lParam),
                       (LPVOID, lpReserved))
APIFW_FORWARD_KERNEL32(EnumSystemLocalesW,
                       (LOCALE_ENUMPROC, lpLocaleEnumProc),
                       (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(FlsGetValue2, (DWORD, dwFlsIndex))
APIFW_FORWARD_KERNEL32(FormatMessageW,
                       (DWORD, dwFlags),
                       (LPCVOID, lpSource),
                       (DWORD, dwMessageId),
                       (DWORD, dwLanguageId),
                       (LPWSTR, lpBuffer),
                       (DWORD, nSize),
                       (va_list*, Arguments))
APIFW_FORWARD_KERNEL32(FreeLibraryAndExitThread,
                       (HMODULE, hLibModule),
                       (DWORD, dwExitCode))
APIFW_FORWARD_KERNEL32(GetActiveProcessorCount, (WORD, GroupNumber))
APIFW_FORWARD_KERNEL32(GetACP)
APIFW_FORWARD_KERNEL32(GetCPInfo, (UINT, CodePage), (LPCPINFO, lpCPInfo))
APIFW_FORWARD_KERNEL32(GetDateFormatEx,
                       (LPCWSTR, lpLocaleName),
                       (DWORD, dwFlags),
                       (const SYSTEMTIME*, lpDate),
                       (LPCWSTR, lpFormat),
                       (LPWSTR, lpDateStr),
                       (int, cchDate),
                       (LPCWSTR, lpCalendar))
APIFW_FORWARD_KERNEL32(GetDateFormatW,
                       (LCID, Locale),
                       (DWORD, dwFlags),
                       (const SYSTEMTIME*, lpDate),
                       (LPCWSTR, lpFormat),
                       (LPWSTR, lpDateStr),
                       (int, cchDate))
APIFW_FORWARD_KERNEL32(GetCommandLineA)
APIFW_FORWARD_KERNEL32(GetCurrentDirectoryW,
                       (DWORD, nBufferLength),
                       (LPWSTR, lpBuffer))
APIFW_FORWARD_KERNEL32(GetDynamicTimeZoneInformation,
                       (PDYNAMIC_TIME_ZONE_INFORMATION, pTimeZoneInformation))
APIFW_FORWARD_KERNEL32(GetFileAttributesExW,
                       (LPCWSTR, lpFileName),
                       (GET_FILEEX_INFO_LEVELS, fInfoLevelId),
                       (LPVOID, lpFileInformation))
APIFW_FORWARD_KERNEL32(GetFileInformationByHandle,
                       (HANDLE, hFile),
                       (LPBY_HANDLE_FILE_INFORMATION, lpFileInformation))
APIFW_FORWARD_KERNEL32(GetFileSizeEx,
                       (HANDLE, hFile),
                       (PLARGE_INTEGER, lpFileSize))
APIFW_FORWARD_KERNEL32(GetFileType, (HANDLE, hFile))
APIFW_FORWARD_KERNEL32(GetFullPathNameW,
                       (LPCWSTR, lpFileName),
                       (DWORD, nBufferLength),
                       (LPWSTR, lpBuffer),
                       (LPWSTR*, lpFilePart))
APIFW_FORWARD_KERNEL32(GetGeoInfoW,
                       (GEOID, GeoId),
                       (GEOTYPE, GeoType),
                       (LPWSTR, lpGeoData),
                       (int, cchGeoData),
                       (LANGID, LangId))
APIFW_FORWARD_KERNEL32(GetHandleInformation,
                       (HANDLE, hObject),
                       (LPDWORD, lpdwFlags))
APIFW_FORWARD_KERNEL32(GetOEMCP)
APIFW_FORWARD_KERNEL32(GetStringTypeW,
                       (DWORD, dwInfoType),
                       (LPCWSTR, lpSrcStr),
                       (int, cchSrc),
                       (LPWORD, lpCharType))
APIFW_FORWARD_KERNEL32(GetTimeFormatEx,
                       (LPCWSTR, lpLocaleName),
                       (DWORD, dwFlags),
                       (const SYSTEMTIME*, lpTime),
                       (LPCWSTR, lpFormat),
                       (LPWSTR, lpTimeStr),
                       (int, cchTime))
APIFW_FORWARD_KERNEL32(GetTimeFormatW,
                       (LCID, Locale),
                       (DWORD, dwFlags),
                       (const SYSTEMTIME*, lpTime),
                       (LPCWSTR, lpFormat),
                       (LPWSTR, lpTimeStr),
                       (int, cchTime))
APIFW_FORWARD_KERNEL32(GetLocaleInfoEx,
                       (LPCWSTR, lpLocaleName),
                       (LCTYPE, LCType),
                       (LPWSTR, lpLCData),
                       (int, cchData))
APIFW_FORWARD_KERNEL32(GetLocaleInfoW,
                       (LCID, Locale),
                       (LCTYPE, LCType),
                       (LPWSTR, lpLCData),
                       (int, cchData))
APIFW_FORWARD_KERNEL32(GetLocalTime, (LPSYSTEMTIME, lpSystemTime))
APIFW_FORWARD_KERNEL32(GetLogicalProcessorInformation,
                       (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, Buffer),
                       (PDWORD, ReturnLength))
APIFW_FORWARD_KERNEL32(GetLogicalProcessorInformationEx,
                       (LOGICAL_PROCESSOR_RELATIONSHIP, RelationshipType),
                       (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Buffer),
                       (PDWORD, ReturnedLength))
APIFW_FORWARD_KERNEL32(GetNativeSystemInfo, (LPSYSTEM_INFO, lpSystemInfo))
APIFW_FORWARD_KERNEL32(GetPriorityClass, (HANDLE, hProcess))
APIFW_FORWARD_KERNEL32(GetProcessHeaps,
                       (DWORD, NumberOfHeaps),
                       (PHANDLE, ProcessHeaps))
APIFW_FORWARD_KERNEL32(GetProcessInformation,
                       (HANDLE, hProcess),
                       (PROCESS_INFORMATION_CLASS, ProcessInformationClass),
                       (LPVOID, ProcessInformation),
                       (DWORD, ProcessInformationLength))
APIFW_FORWARD_KERNEL32(GetProcessMitigationPolicy,
                       (HANDLE, hProcess),
                       (PROCESS_MITIGATION_POLICY, MitigationPolicy),
                       (PVOID, lpBuffer),
                       (SIZE_T, dwLength))
APIFW_FORWARD_KERNEL32(GetProcessTimes,
                       (HANDLE, hProcess),
                       (LPFILETIME, lpCreationTime),
                       (LPFILETIME, lpExitTime),
                       (LPFILETIME, lpKernelTime),
                       (LPFILETIME, lpUserTime))
APIFW_FORWARD_KERNEL32(GetProductInfo,
                       (DWORD, dwOSMajorVersion),
                       (DWORD, dwOSMinorVersion),
                       (DWORD, dwSpMajorVersion),
                       (DWORD, dwSpMinorVersion),
                       (PDWORD, pdwReturnedProductType))
APIFW_FORWARD_KERNEL32(GetQueuedCompletionStatus,
                       (HANDLE, CompletionPort),
                       (LPDWORD, lpNumberOfBytesTransferred),
                       (PULONG_PTR, lpCompletionKey),
                       (LPOVERLAPPED*, lpOverlapped),
                       (DWORD, dwMilliseconds))
APIFW_FORWARD_KERNEL32(GetSystemDefaultLCID)
APIFW_FORWARD_KERNEL32(GetSystemDirectoryA, (LPSTR, lpBuffer), (UINT, uSize))
APIFW_FORWARD_KERNEL32(GetSystemDirectoryW, (LPWSTR, lpBuffer), (UINT, uSize))
APIFW_FORWARD_KERNEL32(GetSystemInfo, (LPSYSTEM_INFO, lpSystemInfo))
APIFW_FORWARD_KERNEL32(GetThreadContext,
                       (HANDLE, hThread),
                       (LPCONTEXT, lpContext))
APIFW_FORWARD_KERNEL32(GetThreadDescription,
                       (HANDLE, hThread),
                       (PWSTR*, ppszThreadDescription))
APIFW_FORWARD_KERNEL32(GetThreadId, (HANDLE, Thread))
APIFW_FORWARD_KERNEL32(GetThreadLocale)
APIFW_FORWARD_KERNEL32(GetThreadPriority, (HANDLE, hThread))
APIFW_FORWARD_KERNEL32(GetThreadPriorityBoost,
                       (HANDLE, hThread),
                       (PBOOL, pDisablePriorityBoost))
APIFW_FORWARD_KERNEL32(GetTimeZoneInformation,
                       (LPTIME_ZONE_INFORMATION, lpTimeZoneInformation))
APIFW_FORWARD_KERNEL32(GetUserDefaultLocaleName,
                       (LPWSTR, lpLocaleName),
                       (int, cchLocaleName))
APIFW_FORWARD_KERNEL32(GetUserGeoID, (GEOCLASS, GeoClass))
// GetVersionExW is marked __declspec(deprecated) in the Windows SDK;
// we intentionally forward it, so suppress the C4996 deprecation error.
#pragma warning(disable : 4996)
APIFW_FORWARD_KERNEL32(GetVersionExW, (LPOSVERSIONINFOW, lpVersionInformation))
APIFW_FORWARD_KERNEL32(GetWindowsDirectoryA, (LPSTR, lpBuffer), (UINT, uSize))
APIFW_FORWARD_KERNEL32(GlobalMemoryStatusEx, (LPMEMORYSTATUSEX, lpBuffer))
APIFW_FORWARD_KERNEL32(IsProcessInJob,
                       (HANDLE, ProcessHandle),
                       (HANDLE, JobHandle),
                       (PBOOL, Result))
APIFW_FORWARD_KERNEL32(IsValidLocaleName, (LPCWSTR, lpLocaleName))
APIFW_FORWARD_KERNEL32(IsValidCodePage, (UINT, CodePage))
APIFW_FORWARD_KERNEL32(IsValidLocale, (LCID, Locale), (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(IsWow64Process,
                       (HANDLE, hProcess),
                       (PBOOL, Wow64Process))
APIFW_FORWARD_KERNEL32(IsWow64Process2,
                       (HANDLE, hProcess),
                       (USHORT*, pProcessMachine),
                       (USHORT*, pNativeMachine))
APIFW_FORWARD_KERNEL32(K32EnumProcessModules,
                       (HANDLE, hProcess),
                       (HMODULE*, lphModule),
                       (DWORD, cb),
                       (LPDWORD, lpcbNeeded))
APIFW_FORWARD_KERNEL32(K32GetModuleInformation,
                       (HANDLE, hProcess),
                       (HMODULE, hModule),
                       (LPMODULEINFO, lpmodinfo),
                       (DWORD, cb))
APIFW_FORWARD_KERNEL32(K32GetProcessMemoryInfo,
                       (HANDLE, Process),
                       (PPROCESS_MEMORY_COUNTERS, ppsmemCounters),
                       (DWORD, cb))
APIFW_FORWARD_KERNEL32(K32QueryWorkingSetEx,
                       (HANDLE, hProcess),
                       (PVOID, pv),
                       (DWORD, cb))
APIFW_FORWARD_KERNEL32(LCIDToLocaleName,
                       (LCID, Locale),
                       (LPWSTR, lpName),
                       (int, cchName),
                       (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(LCMapStringEx,
                       (LPCWSTR, lpLocaleName),
                       (DWORD, dwMapFlags),
                       (LPCWSTR, lpSrcStr),
                       (int, cchSrc),
                       (LPWSTR, lpDestStr),
                       (int, cchDest),
                       (LPNLSVERSIONINFO, lpVersionInformation),
                       (LPVOID, lpReserved),
                       (LPARAM, sortHandle))
APIFW_FORWARD_KERNEL32(LCMapStringW,
                       (LCID, Locale),
                       (DWORD, dwMapFlags),
                       (LPCWSTR, lpSrcStr),
                       (int, cchSrc),
                       (LPWSTR, lpDestStr),
                       (int, cchDest))
APIFW_FORWARD_KERNEL32(LocalFree, (HLOCAL, hMem))
APIFW_FORWARD_KERNEL32(LocaleNameToLCID, (LPCWSTR, lpName), (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(lstrcmpiA, (LPCSTR, lpString1), (LPCSTR, lpString2))
APIFW_FORWARD_KERNEL32(LockFileEx,
                       (HANDLE, hFile),
                       (DWORD, dwFlags),
                       (DWORD, dwReserved),
                       (DWORD, nNumberOfBytesToLockLow),
                       (DWORD, nNumberOfBytesToLockHigh),
                       (LPOVERLAPPED, lpOverlapped))
APIFW_FORWARD_KERNEL32(Module32FirstW,
                       (HANDLE, hSnapshot),
                       (LPMODULEENTRY32W, lpme))
APIFW_FORWARD_KERNEL32(Module32NextW,
                       (HANDLE, hSnapshot),
                       (LPMODULEENTRY32W, lpme))
APIFW_FORWARD_KERNEL32(OpenProcess,
                       (DWORD, dwDesiredAccess),
                       (BOOL, bInheritHandle),
                       (DWORD, dwProcessId))
APIFW_FORWARD_KERNEL32(OpenProcessToken,
                       (HANDLE, ProcessHandle),
                       (DWORD, DesiredAccess),
                       (PHANDLE, TokenHandle))
APIFW_FORWARD_KERNEL32(PostQueuedCompletionStatus,
                       (HANDLE, CompletionPort),
                       (DWORD, dwNumberOfBytesTransferred),
                       (ULONG_PTR, CompletionKey),
                       (LPOVERLAPPED, lpOverlapped))
APIFW_FORWARD_KERNEL32(QueryThreadCycleTime,
                       (HANDLE, ThreadHandle),
                       (PULONG64, CycleTime))
APIFW_FORWARD_KERNEL32(RegCloseKey, (HKEY, hKey))
APIFW_FORWARD_KERNEL32(RegisterWaitForSingleObject,
                       (PHANDLE, phNewWaitObject),
                       (HANDLE, hObject),
                       (WAITORTIMERCALLBACK, Callback),
                       (PVOID, Context),
                       (ULONG, dwMilliseconds),
                       (ULONG, dwFlags))
APIFW_FORWARD_KERNEL32(RegNotifyChangeKeyValue,
                       (HKEY, hKey),
                       (BOOL, bWatchSubtree),
                       (DWORD, dwNotifyFilter),
                       (HANDLE, hEvent),
                       (BOOL, fAsynchronous))
APIFW_FORWARD_KERNEL32(RegOpenKeyExW,
                       (HKEY, hKey),
                       (LPCWSTR, lpSubKey),
                       (DWORD, ulOptions),
                       (REGSAM, samDesired),
                       (PHKEY, phkResult))
APIFW_FORWARD_KERNEL32(RegQueryValueExW,
                       (HKEY, hKey),
                       (LPCWSTR, lpValueName),
                       (LPDWORD, lpReserved),
                       (LPDWORD, lpType),
                       (LPBYTE, lpData),
                       (LPDWORD, lpcbData))
APIFW_FORWARD_KERNEL32(ReleaseSemaphore,
                       (HANDLE, hSemaphore),
                       (LONG, lReleaseCount),
                       (LPLONG, lpPreviousCount))
APIFW_FORWARD_KERNEL32(ResumeThread, (HANDLE, hThread))
APIFW_FORWARD_KERNEL32(SetConsoleCtrlHandler,
                       (PHANDLER_ROUTINE, HandlerRoutine),
                       (BOOL, Add))
APIFW_FORWARD_KERNEL32(SetCurrentDirectoryW, (LPCWSTR, lpPathName))
APIFW_FORWARD_KERNEL32(SetEndOfFile, (HANDLE, hFile))
APIFW_FORWARD_KERNEL32(SetErrorMode, (UINT, uMode))
APIFW_FORWARD_KERNEL32(SetNamedPipeHandleState,
                       (HANDLE, hNamedPipe),
                       (LPDWORD, lpMode),
                       (LPDWORD, lpMaxCollectionCount),
                       (LPDWORD, lpCollectDataTimeout))
APIFW_FORWARD_KERNEL32(SetFilePointer,
                       (HANDLE, hFile),
                       (LONG, lDistanceToMove),
                       (PLONG, lpDistanceToMoveHigh),
                       (DWORD, dwMoveMethod))
APIFW_FORWARD_KERNEL32(SetFilePointerEx,
                       (HANDLE, hFile),
                       (LARGE_INTEGER, liDistanceToMove),
                       (PLARGE_INTEGER, lpNewFilePointer),
                       (DWORD, dwMoveMethod))
APIFW_FORWARD_KERNEL32(SetFileTime,
                       (HANDLE, hFile),
                       (const FILETIME*, lpCreationTime),
                       (const FILETIME*, lpLastAccessTime),
                       (const FILETIME*, lpLastWriteTime))
APIFW_FORWARD_KERNEL32(SetProcessMitigationPolicy,
                       (PROCESS_MITIGATION_POLICY, MitigationPolicy),
                       (PVOID, lpBuffer),
                       (SIZE_T, dwLength))
APIFW_FORWARD_KERNEL32(SetProcessShutdownParameters,
                       (DWORD, dwLevel),
                       (DWORD, dwFlags))
APIFW_FORWARD_KERNEL32(SetThreadDescription,
                       (HANDLE, hThread),
                       (PCWSTR, lpThreadDescription))
APIFW_FORWARD_KERNEL32(SetThreadInformation,
                       (HANDLE, hThread),
                       (THREAD_INFORMATION_CLASS, ThreadInformationClass),
                       (LPVOID, ThreadInformation),
                       (DWORD, ThreadInformationSize))
APIFW_FORWARD_KERNEL32(SetThreadPriority, (HANDLE, hThread), (int, nPriority))
APIFW_FORWARD_KERNEL32(SetThreadPriorityBoost,
                       (HANDLE, hThread),
                       (BOOL, bDisablePriorityBoost))
APIFW_FORWARD_KERNEL32(SuspendThread, (HANDLE, hThread))
APIFW_FORWARD_KERNEL32(SwitchToThread)
APIFW_FORWARD_KERNEL32(TransactNamedPipe,
                       (HANDLE, hNamedPipe),
                       (LPVOID, lpInBuffer),
                       (DWORD, nInBufferSize),
                       (LPVOID, lpOutBuffer),
                       (DWORD, nOutBufferSize),
                       (LPDWORD, lpBytesRead),
                       (LPOVERLAPPED, lpOverlapped))
APIFW_FORWARD_KERNEL32(UnlockFileEx,
                       (HANDLE, hFile),
                       (DWORD, dwReserved),
                       (DWORD, nNumberOfBytesToUnlockLow),
                       (DWORD, nNumberOfBytesToUnlockHigh),
                       (LPOVERLAPPED, lpOverlapped))
APIFW_FORWARD_KERNEL32(UnregisterWaitEx,
                       (HANDLE, WaitHandle),
                       (HANDLE, CompletionEvent))
APIFW_FORWARD_KERNEL32(VerifyVersionInfoW,
                       (LPOSVERSIONINFOEXW, lpVersionInformation),
                       (DWORD, dwTypeMask),
                       (ULONGLONG, dwlConditionMask))
APIFW_FORWARD_KERNEL32(VerSetConditionMask,
                       (ULONGLONG, ConditionMask),
                       (DWORD, TypeMask),
                       (BYTE, Condition))
APIFW_FORWARD_KERNEL32(WaitNamedPipeW,
                       (LPCWSTR, lpNamedPipeName),
                       (DWORD, nTimeOut))
APIFW_FORWARD_KERNEL32(WerRegisterRuntimeExceptionModule,
                       (PCWSTR, pwszOutOfProcessCallbackDll),
                       (PVOID, pContext))
APIFW_FORWARD_KERNEL32(Wow64GetThreadContext,
                       (HANDLE, hThread),
                       (PWOW64_CONTEXT, lpContext))

// --- ntdll passthroughs ---

APIFW_FORWARD_NTDLL(LdrRegisterDllNotification,
                    (ULONG, flags),
                    (PLDR_DLL_NOTIFICATION_FUNCTION, notification_function),
                    (PVOID, context),
                    (PVOID*, cookie))
APIFW_FORWARD_NTDLL(NtClose, (HANDLE, Handle))
APIFW_FORWARD_NTDLL(NtCreateKey,
                    (PHANDLE, KeyHandle),
                    (ACCESS_MASK, DesiredAccess),
                    (POBJECT_ATTRIBUTES, ObjectAttributes),
                    (ULONG, TitleIndex),
                    (PUNICODE_STRING, Class),
                    (ULONG, CreateOptions),
                    (PULONG, Disposition))
APIFW_FORWARD_NTDLL(NtOpenKeyEx,
                    (PHANDLE, KeyHandle),
                    (ACCESS_MASK, DesiredAccess),
                    (POBJECT_ATTRIBUTES, ObjectAttributes),
                    (ULONG, OpenOptions))
APIFW_FORWARD_NTDLL(NtQueryInformationProcess,
                    (HANDLE, ProcessHandle),
                    (PROCESSINFOCLASS, ProcessInformationClass),
                    (PVOID, ProcessInformation),
                    (ULONG, ProcessInformationLength),
                    (PULONG, ReturnLength))
APIFW_FORWARD_NTDLL(NtQueryObject,
                    (HANDLE, Handle),
                    (OBJECT_INFORMATION_CLASS, ObjectInformationClass),
                    (PVOID, ObjectInformation),
                    (ULONG, ObjectInformationLength),
                    (PULONG, ReturnLength))
APIFW_FORWARD_NTDLL(NtQuerySection,
                    (HANDLE, SectionHandle),
                    (SECTION_INFORMATION_CLASS, SectionInformationClass),
                    (PVOID, SectionInformation),
                    (ULONG, SectionInformationLength),
                    (PULONG, ReturnLength))
APIFW_FORWARD_NTDLL(NtQueryValueKey,
                    (HANDLE, KeyHandle),
                    (PUNICODE_STRING, ValueName),
                    (KEY_VALUE_INFORMATION_CLASS, KeyValueInformationClass),
                    (PVOID, KeyValueInformation),
                    (ULONG, KeyValueInformationLength),
                    (PULONG, ResultLength))
APIFW_FORWARD_NTDLL(NtSetValueKey,
                    (HANDLE, KeyHandle),
                    (PCUNICODE_STRING, ValueName),
                    (ULONG, TitleIndex),
                    (ULONG, Type),
                    (PVOID, Data),
                    (ULONG, DataSize))
APIFW_FORWARD_NTDLL(RtlFormatCurrentUserKeyPath, (PUNICODE_STRING, KeyPath))
APIFW_FORWARD_NTDLL(RtlFreeUnicodeString, (PUNICODE_STRING, UnicodeString))
APIFW_FORWARD_NTDLL(RtlInitUnicodeString,
                    (PUNICODE_STRING, DestinationString),
                    (PCWSTR, SourceString))

// --- winmm passthroughs ---

APIFW_FORWARD_WINMM(timeBeginPeriod, (UINT, uPeriod))
APIFW_FORWARD_WINMM(timeEndPeriod, (UINT, uPeriod))

}  // extern "C"
