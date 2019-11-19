// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For information about interceptions as a whole see
// http://dev.chromium.org/developers/design-documents/sandbox .

#include "sandbox/win/src/interception.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_rand.h"
#include "sandbox/win/src/service_resolver.h"
#include "sandbox/win/src/target_interceptions.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

// Standard allocation granularity and page size for Windows.
const size_t kAllocGranularity = 65536;
const size_t kPageSize = 4096;

}  // namespace

namespace internal {

// Find a random offset within 64k and aligned to ceil(log2(size)).
size_t GetGranularAlignedRandomOffset(size_t size) {
  CHECK_LE(size, kAllocGranularity);
  unsigned int offset;

  do {
    GetRandom(&offset);
    offset &= (kAllocGranularity - 1);
  } while (offset > (kAllocGranularity - size));

  // Find an alignment between 64 and the page size (4096).
  size_t align_size = kPageSize;
  for (size_t new_size = align_size / 2; new_size >= size; new_size /= 2) {
    align_size = new_size;
  }
  return offset & ~(align_size - 1);
}

}  // namespace internal

SANDBOX_INTERCEPT SharedMemory* g_interceptions;

// Table of the unpatched functions that we intercept. Mapped from the parent.
SANDBOX_INTERCEPT OriginalFunctions g_originals = {nullptr};

// Magic constant that identifies that this function is not to be patched.
const char kUnloadDLLDummyFunction[] = "@";

InterceptionManager::InterceptionData::InterceptionData() {}

InterceptionManager::InterceptionData::InterceptionData(
    const InterceptionData& other) = default;

InterceptionManager::InterceptionData::~InterceptionData() {}

InterceptionManager::InterceptionManager(TargetProcess* child_process,
                                         bool relaxed)
    : child_(child_process), names_used_(false), relaxed_(relaxed) {
  child_->AddRef();
}
InterceptionManager::~InterceptionManager() {
  child_->Release();
}

bool InterceptionManager::AddToPatchedFunctions(
    const wchar_t* dll_name,
    const char* function_name,
    InterceptionType interception_type,
    const void* replacement_code_address,
    InterceptorId id) {
  InterceptionData function;
  function.type = interception_type;
  function.id = id;
  function.dll = dll_name;
  function.function = function_name;
  function.interceptor_address = replacement_code_address;

  interceptions_.push_back(function);
  return true;
}

bool InterceptionManager::AddToPatchedFunctions(
    const wchar_t* dll_name,
    const char* function_name,
    InterceptionType interception_type,
    const char* replacement_function_name,
    InterceptorId id) {
  InterceptionData function;
  function.type = interception_type;
  function.id = id;
  function.dll = dll_name;
  function.function = function_name;
  function.interceptor = replacement_function_name;
  function.interceptor_address = nullptr;

  interceptions_.push_back(function);
  names_used_ = true;
  return true;
}

bool InterceptionManager::AddToUnloadModules(const wchar_t* dll_name) {
  InterceptionData module_to_unload;
  module_to_unload.type = INTERCEPTION_UNLOAD_MODULE;
  module_to_unload.dll = dll_name;
  // The next two are dummy values that make the structures regular, instead
  // of having special cases. They should not be used.
  module_to_unload.function = kUnloadDLLDummyFunction;
  module_to_unload.interceptor_address = reinterpret_cast<void*>(1);

  interceptions_.push_back(module_to_unload);
  return true;
}

ResultCode InterceptionManager::InitializeInterceptions() {
  if (interceptions_.empty())
    return SBOX_ALL_OK;  // Nothing to do here

  size_t buffer_bytes = GetBufferSize();
  std::unique_ptr<char[]> local_buffer(new char[buffer_bytes]);

  if (!SetupConfigBuffer(local_buffer.get(), buffer_bytes))
    return SBOX_ERROR_CANNOT_SETUP_INTERCEPTION_CONFIG_BUFFER;

  void* remote_buffer;
  if (!CopyToChildMemory(child_->Process(), local_buffer.get(), buffer_bytes,
                         &remote_buffer))
    return SBOX_ERROR_CANNOT_COPY_DATA_TO_CHILD;

  bool hot_patch_needed = (0 != buffer_bytes);
  ResultCode rc = PatchNtdll(hot_patch_needed);

  if (rc != SBOX_ALL_OK)
    return rc;

  g_interceptions = reinterpret_cast<SharedMemory*>(remote_buffer);
  rc = child_->TransferVariable("g_interceptions", &g_interceptions,
                                sizeof(g_interceptions));
  return rc;
}

size_t InterceptionManager::GetBufferSize() const {
  std::set<std::wstring> dlls;
  size_t buffer_bytes = 0;

  for (const auto& interception : interceptions_) {
    // skip interceptions that are performed from the parent
    if (!IsInterceptionPerformedByChild(interception))
      continue;

    if (!dlls.count(interception.dll)) {
      // NULL terminate the dll name on the structure
      size_t dll_name_bytes = (interception.dll.size() + 1) * sizeof(wchar_t);

      // include the dll related size
      buffer_bytes += RoundUpToMultiple(
          offsetof(DllPatchInfo, dll_name) + dll_name_bytes, sizeof(size_t));
      dlls.insert(interception.dll);
    }

    // we have to NULL terminate the strings on the structure
    size_t strings_chars =
        interception.function.size() + interception.interceptor.size() + 2;

    // a new FunctionInfo is required per function
    size_t record_bytes = offsetof(FunctionInfo, function) + strings_chars;
    record_bytes = RoundUpToMultiple(record_bytes, sizeof(size_t));
    buffer_bytes += record_bytes;
  }

  if (0 != buffer_bytes)
    // add the part of SharedMemory that we have not counted yet
    buffer_bytes += offsetof(SharedMemory, dll_list);

  return buffer_bytes;
}

// Basically, walk the list of interceptions moving them to the config buffer,
// but keeping together all interceptions that belong to the same dll.
// The config buffer is a local buffer, not the one allocated on the child.
bool InterceptionManager::SetupConfigBuffer(void* buffer, size_t buffer_bytes) {
  if (0 == buffer_bytes)
    return true;

  DCHECK(buffer_bytes > sizeof(SharedMemory));

  SharedMemory* shared_memory = reinterpret_cast<SharedMemory*>(buffer);
  DllPatchInfo* dll_info = shared_memory->dll_list;
  int num_dlls = 0;

  shared_memory->interceptor_base =
      names_used_ ? child_->MainModule() : nullptr;

  buffer_bytes -= offsetof(SharedMemory, dll_list);
  buffer = dll_info;

  std::list<InterceptionData>::iterator it = interceptions_.begin();
  for (; it != interceptions_.end();) {
    // skip interceptions that are performed from the parent
    if (!IsInterceptionPerformedByChild(*it)) {
      ++it;
      continue;
    }

    const std::wstring dll = it->dll;
    if (!SetupDllInfo(*it, &buffer, &buffer_bytes))
      return false;

    // walk the interceptions from this point, saving the ones that are
    // performed on this dll, and removing the entry from the list.
    // advance the iterator before removing the element from the list
    std::list<InterceptionData>::iterator rest = it;
    for (; rest != interceptions_.end();) {
      if (rest->dll == dll) {
        if (!SetupInterceptionInfo(*rest, &buffer, &buffer_bytes, dll_info))
          return false;
        if (it == rest)
          ++it;
        rest = interceptions_.erase(rest);
      } else {
        ++rest;
      }
    }
    dll_info = reinterpret_cast<DllPatchInfo*>(buffer);
    ++num_dlls;
  }

  shared_memory->num_intercepted_dlls = num_dlls;
  return true;
}

// Fills up just the part that depends on the dll, not the info that depends on
// the actual interception.
bool InterceptionManager::SetupDllInfo(const InterceptionData& data,
                                       void** buffer,
                                       size_t* buffer_bytes) const {
  DCHECK(buffer_bytes);
  DCHECK(buffer);
  DCHECK(*buffer);

  DllPatchInfo* dll_info = reinterpret_cast<DllPatchInfo*>(*buffer);

  // the strings have to be zero terminated
  size_t required = offsetof(DllPatchInfo, dll_name) +
                    (data.dll.size() + 1) * sizeof(wchar_t);
  required = RoundUpToMultiple(required, sizeof(size_t));
  if (*buffer_bytes < required)
    return false;

  *buffer_bytes -= required;
  *buffer = reinterpret_cast<char*>(*buffer) + required;

  // set up the dll info to be what we know about it at this time
  dll_info->unload_module = (data.type == INTERCEPTION_UNLOAD_MODULE);
  dll_info->record_bytes = required;
  dll_info->offset_to_functions = required;
  dll_info->num_functions = 0;
  data.dll.copy(dll_info->dll_name, data.dll.size());
  dll_info->dll_name[data.dll.size()] = L'\0';

  return true;
}

bool InterceptionManager::SetupInterceptionInfo(const InterceptionData& data,
                                                void** buffer,
                                                size_t* buffer_bytes,
                                                DllPatchInfo* dll_info) const {
  DCHECK(buffer_bytes);
  DCHECK(buffer);
  DCHECK(*buffer);

  if ((dll_info->unload_module) && (data.function != kUnloadDLLDummyFunction)) {
    // Can't specify a dll for both patch and unload.
    NOTREACHED();
  }

  FunctionInfo* function = reinterpret_cast<FunctionInfo*>(*buffer);

  size_t name_bytes = data.function.size();
  size_t interceptor_bytes = data.interceptor.size();

  // the strings at the end of the structure are zero terminated
  size_t required =
      offsetof(FunctionInfo, function) + name_bytes + interceptor_bytes + 2;
  required = RoundUpToMultiple(required, sizeof(size_t));
  if (*buffer_bytes < required)
    return false;

  // update the caller's values
  *buffer_bytes -= required;
  *buffer = reinterpret_cast<char*>(*buffer) + required;

  function->record_bytes = required;
  function->type = data.type;
  function->id = data.id;
  function->interceptor_address = data.interceptor_address;
  char* names = function->function;

  data.function.copy(names, name_bytes);
  names += name_bytes;
  *names++ = '\0';

  // interceptor follows the function_name
  data.interceptor.copy(names, interceptor_bytes);
  names += interceptor_bytes;
  *names++ = '\0';

  // update the dll table
  dll_info->num_functions++;
  dll_info->record_bytes += required;

  return true;
}

// Only return true if the child should be able to perform this interception.
bool InterceptionManager::IsInterceptionPerformedByChild(
    const InterceptionData& data) const {
  if (INTERCEPTION_INVALID == data.type)
    return false;

  if (INTERCEPTION_SERVICE_CALL == data.type)
    return false;

  if (data.type >= INTERCEPTION_LAST)
    return false;

  std::wstring ntdll(kNtdllName);
  if (ntdll == data.dll)
    return false;  // ntdll has to be intercepted from the parent

  return true;
}

ResultCode InterceptionManager::PatchNtdll(bool hot_patch_needed) {
  // Maybe there is nothing to do
  if (!hot_patch_needed && interceptions_.empty())
    return SBOX_ALL_OK;

  if (hot_patch_needed) {
#if defined(SANDBOX_EXPORTS)
// Make sure the functions are not excluded by the linker.
#if defined(_WIN64)
#pragma comment(linker, "/include:TargetNtMapViewOfSection64")
#pragma comment(linker, "/include:TargetNtUnmapViewOfSection64")
#else
#pragma comment(linker, "/include:_TargetNtMapViewOfSection@44")
#pragma comment(linker, "/include:_TargetNtUnmapViewOfSection@12")
#endif
#endif  // defined(SANDBOX_EXPORTS)
    ADD_NT_INTERCEPTION(NtMapViewOfSection, MAP_VIEW_OF_SECTION_ID, 44);
    ADD_NT_INTERCEPTION(NtUnmapViewOfSection, UNMAP_VIEW_OF_SECTION_ID, 12);
  }

  // Reserve a full 64k memory range in the child process.
  HANDLE child = child_->Process();
  BYTE* thunk_base = reinterpret_cast<BYTE*>(::VirtualAllocEx(
      child, nullptr, kAllocGranularity, MEM_RESERVE, PAGE_NOACCESS));

  // Find an aligned, random location within the reserved range.
  size_t thunk_bytes =
      interceptions_.size() * sizeof(ThunkData) + sizeof(DllInterceptionData);
  size_t thunk_offset = internal::GetGranularAlignedRandomOffset(thunk_bytes);

  // Split the base and offset along page boundaries.
  thunk_base += thunk_offset & ~(kPageSize - 1);
  thunk_offset &= kPageSize - 1;

  // Make an aligned, padded allocation, and move the pointer to our chunk.
  size_t thunk_bytes_padded = (thunk_bytes + kPageSize - 1) & ~(kPageSize - 1);
  thunk_base = reinterpret_cast<BYTE*>(
      ::VirtualAllocEx(child, thunk_base, thunk_bytes_padded, MEM_COMMIT,
                       PAGE_EXECUTE_READWRITE));
  CHECK(thunk_base);  // If this fails we'd crash anyway on an invalid access.
  DllInterceptionData* thunks =
      reinterpret_cast<DllInterceptionData*>(thunk_base + thunk_offset);

  DllInterceptionData dll_data;
  dll_data.data_bytes = thunk_bytes;
  dll_data.num_thunks = 0;
  dll_data.used_bytes = offsetof(DllInterceptionData, thunks);

  // Reset all helpers for a new child.
  memset(g_originals, 0, sizeof(g_originals));

  // this should write all the individual thunks to the child's memory
  ResultCode rc = PatchClientFunctions(thunks, thunk_bytes, &dll_data);

  if (rc != SBOX_ALL_OK)
    return rc;

  // and now write the first part of the table to the child's memory
  SIZE_T written;
  bool ok =
      !!::WriteProcessMemory(child, thunks, &dll_data,
                             offsetof(DllInterceptionData, thunks), &written);

  if (!ok || (offsetof(DllInterceptionData, thunks) != written))
    return SBOX_ERROR_CANNOT_WRITE_INTERCEPTION_THUNK;

  // Attempt to protect all the thunks, but ignore failure
  DWORD old_protection;
  ::VirtualProtectEx(child, thunks, thunk_bytes, PAGE_EXECUTE_READ,
                     &old_protection);

  ResultCode ret =
      child_->TransferVariable("g_originals", g_originals, sizeof(g_originals));
  return ret;
}

ResultCode InterceptionManager::PatchClientFunctions(
    DllInterceptionData* thunks,
    size_t thunk_bytes,
    DllInterceptionData* dll_data) {
  DCHECK(thunks);
  DCHECK(dll_data);

  HMODULE ntdll_base = ::GetModuleHandle(kNtdllName);
  if (!ntdll_base)
    return SBOX_ERROR_NO_HANDLE;

  char* interceptor_base = nullptr;

#if defined(SANDBOX_EXPORTS)
  interceptor_base = reinterpret_cast<char*>(child_->MainModule());
  base::ScopedNativeLibrary local_interceptor(::LoadLibrary(child_->Name()));
#endif  // defined(SANDBOX_EXPORTS)

  std::unique_ptr<ServiceResolverThunk> thunk;
#if defined(_WIN64)
  thunk.reset(new ServiceResolverThunk(child_->Process(), relaxed_));
#else
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->wow64_status() == base::win::OSInfo::WOW64_ENABLED) {
    if (os_info->version() >= base::win::Version::WIN10)
      thunk.reset(new Wow64W10ResolverThunk(child_->Process(), relaxed_));
    else if (os_info->version() >= base::win::Version::WIN8)
      thunk.reset(new Wow64W8ResolverThunk(child_->Process(), relaxed_));
    else
      thunk.reset(new Wow64ResolverThunk(child_->Process(), relaxed_));
  } else if (os_info->version() >= base::win::Version::WIN8) {
    thunk.reset(new Win8ResolverThunk(child_->Process(), relaxed_));
  } else {
    thunk.reset(new ServiceResolverThunk(child_->Process(), relaxed_));
  }
#endif

  for (auto interception : interceptions_) {
    const std::wstring ntdll(kNtdllName);
    if (interception.dll != ntdll)
      return SBOX_ERROR_BAD_PARAMS;

    if (INTERCEPTION_SERVICE_CALL != interception.type)
      return SBOX_ERROR_BAD_PARAMS;

#if defined(SANDBOX_EXPORTS)
    // We may be trying to patch by function name.
    if (!interception.interceptor_address) {
      const char* address;
      NTSTATUS ret = thunk->ResolveInterceptor(
          local_interceptor.get(), interception.interceptor.c_str(),
          reinterpret_cast<const void**>(&address));
      if (!NT_SUCCESS(ret)) {
        ::SetLastError(GetLastErrorFromNtStatus(ret));
        return SBOX_ERROR_CANNOT_RESOLVE_INTERCEPTION_THUNK;
      }

      // Translate the local address to an address on the child.
      interception.interceptor_address =
          interceptor_base +
          (address - reinterpret_cast<char*>(local_interceptor.get()));
    }
#endif  // defined(SANDBOX_EXPORTS)
    NTSTATUS ret = thunk->Setup(
        ntdll_base, interceptor_base, interception.function.c_str(),
        interception.interceptor.c_str(), interception.interceptor_address,
        &thunks->thunks[dll_data->num_thunks],
        thunk_bytes - dll_data->used_bytes, nullptr);
    if (!NT_SUCCESS(ret)) {
      ::SetLastError(GetLastErrorFromNtStatus(ret));
      return SBOX_ERROR_CANNOT_SETUP_INTERCEPTION_THUNK;
    }

    DCHECK(!g_originals[interception.id]);
    g_originals[interception.id] = &thunks->thunks[dll_data->num_thunks];

    dll_data->num_thunks++;
    dll_data->used_bytes += sizeof(ThunkData);
  }

  return SBOX_ALL_OK;
}

}  // namespace sandbox
