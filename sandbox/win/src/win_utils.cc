// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win_utils.h"

#include <windows.h>

#include <ntstatus.h>
#include <psapi.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

NTSTATUS WrapQueryObject(HANDLE handle,
                         OBJECT_INFORMATION_CLASS info_class,
                         std::vector<uint8_t>& buffer,
                         PULONG reqd) {
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
    return STATUS_INVALID_PARAMETER;
  NtQueryObjectFunction NtQueryObject = sandbox::GetNtExports()->QueryObject;
  ULONG size = static_cast<ULONG>(buffer.size());
  __try {
    return NtQueryObject(handle, info_class, buffer.data(), size, reqd);
  } __except (GetExceptionCode() == STATUS_INVALID_HANDLE
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return STATUS_INVALID_PARAMETER;
  }
}

// `hint` is used for the initial call to NtQueryObject. Note that some data
// in the returned vector might be unused.
std::unique_ptr<std::vector<uint8_t>> QueryObjectInformation(
    HANDLE handle,
    OBJECT_INFORMATION_CLASS info_class,
    ULONG hint) {
  // Internal pointers in this buffer cannot move about so cannot just return
  // the vector.
  auto data = std::make_unique<std::vector<uint8_t>>(hint);
  ULONG req = 0;
  NTSTATUS ret = WrapQueryObject(handle, info_class, *data, &req);
  if (ret == STATUS_INFO_LENGTH_MISMATCH || ret == STATUS_BUFFER_TOO_SMALL ||
      ret == STATUS_BUFFER_OVERFLOW) {
    data->resize(req);
    ret = WrapQueryObject(handle, info_class, *data, nullptr);
  }
  if (!NT_SUCCESS(ret))
    return nullptr;
  return data;
}

}  // namespace

namespace sandbox {

bool IsPipe(const std::wstring& path) {
  std::wstring prefix = sandbox::kNTPrefix;
  prefix += L"pipe\\";
  return base::StartsWith(path, prefix, base::CompareCase::INSENSITIVE_ASCII);
}

std::optional<std::wstring> GetNtPathFromWin32Path(const std::wstring& path) {
  base::win::ScopedHandle file(::CreateFileW(
      path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (!file.is_valid()) {
    return std::nullopt;
  }
  return GetPathFromHandle(file.get());
}

std::optional<std::wstring> GetPathFromHandle(HANDLE handle) {
  auto buffer = QueryObjectInformation(handle, ObjectNameInformation, 512);
  if (!buffer)
    return std::nullopt;
  OBJECT_NAME_INFORMATION* name =
      reinterpret_cast<OBJECT_NAME_INFORMATION*>(buffer->data());
  return std::wstring(
      name->ObjectName.Buffer,
      name->ObjectName.Length / sizeof(name->ObjectName.Buffer[0]));
}

std::optional<std::wstring> GetTypeNameFromHandle(HANDLE handle) {
  // No typename is currently longer than 32 characters on Windows 11, so use a
  // hint of 128 bytes.
  auto buffer = QueryObjectInformation(handle, ObjectTypeInformation, 128);
  if (!buffer)
    return std::nullopt;
  OBJECT_TYPE_INFORMATION* name =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(buffer->data());
  return std::wstring(name->Name.Buffer,
                      name->Name.Length / sizeof(name->Name.Buffer[0]));
}

bool CopyToChildMemory(HANDLE child,
                       const void* local_buffer,
                       size_t buffer_bytes,
                       void** remote_buffer) {
  DCHECK(remote_buffer);
  if (0 == buffer_bytes) {
    *remote_buffer = nullptr;
    return true;
  }

  // Allocate memory in the target process without specifying the address
  void* remote_data = ::VirtualAllocEx(child, nullptr, buffer_bytes, MEM_COMMIT,
                                       PAGE_READWRITE);
  if (!remote_data)
    return false;

  SIZE_T bytes_written;
  bool success = ::WriteProcessMemory(child, remote_data, local_buffer,
                                      buffer_bytes, &bytes_written);
  if (!success || bytes_written != buffer_bytes) {
    ::VirtualFreeEx(child, remote_data, 0, MEM_RELEASE);
    return false;
  }

  *remote_buffer = remote_data;

  return true;
}

DWORD GetLastErrorFromNtStatus(NTSTATUS status) {
  return GetNtExports()->RtlNtStatusToDosError(status);
}

// This function uses the undocumented PEB ImageBaseAddress field to extract
// the base address of the new process.
void* GetProcessBaseAddress(HANDLE process) {
  PROCESS_BASIC_INFORMATION process_basic_info = {};
  NTSTATUS status = GetNtExports()->QueryInformationProcess(
      process, ProcessBasicInformation, &process_basic_info,
      sizeof(process_basic_info), nullptr);
  if (STATUS_SUCCESS != status)
    return nullptr;

  NT_PEB peb = {};
  SIZE_T bytes_read = 0;
  if (!::ReadProcessMemory(process, process_basic_info.PebBaseAddress, &peb,
                           sizeof(peb), &bytes_read) ||
      (sizeof(peb) != bytes_read)) {
    return nullptr;
  }

  void* base_address = peb.ImageBaseAddress;
  char magic[2] = {};
  if (!::ReadProcessMemory(process, base_address, magic, sizeof(magic),
                           &bytes_read) ||
      (sizeof(magic) != bytes_read)) {
    return nullptr;
  }

  if (magic[0] != 'M' || magic[1] != 'Z')
    return nullptr;

  return base_address;
}

std::optional<ProcessHandleMap> GetCurrentProcessHandles() {
  DWORD handle_count;
  if (!::GetProcessHandleCount(::GetCurrentProcess(), &handle_count))
    return std::nullopt;

  // The system call will return only handles up to the buffer size so add a
  // margin of error of an additional 1000 handles.
  std::vector<char> buffer((handle_count + 1000) * sizeof(uint32_t));
  DWORD return_length;
  NTSTATUS status = GetNtExports()->QueryInformationProcess(
      ::GetCurrentProcess(), ProcessHandleTable, buffer.data(),
      static_cast<ULONG>(buffer.size()), &return_length);

  if (!NT_SUCCESS(status)) {
    ::SetLastError(GetLastErrorFromNtStatus(status));
    return std::nullopt;
  }
  DCHECK(buffer.size() >= return_length);
  DCHECK((buffer.size() % sizeof(uint32_t)) == 0);
  ProcessHandleMap handle_map;
  const uint32_t* handle_values = reinterpret_cast<uint32_t*>(buffer.data());
  size_t count = return_length / sizeof(uint32_t);
  for (size_t index = 0; index < count; ++index) {
    HANDLE handle = base::win::Uint32ToHandle(handle_values[index]);
    auto type_name = GetTypeNameFromHandle(handle);
    if (type_name)
      handle_map[type_name.value()].push_back(handle);
  }
  return handle_map;
}

bool ContainsNulCharacter(std::wstring_view str) {
  wchar_t nul = '\0';
  return str.find_first_of(nul) != std::wstring::npos;
}

}  // namespace sandbox
