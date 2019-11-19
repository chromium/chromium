// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_nt_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/win/pe_image.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt;

}  // namespace sandbox

namespace {

#if defined(_WIN64)
// Align a pointer to the next allocation granularity boundary.
inline char* AlignToBoundary(void* ptr, size_t increment) {
  const size_t kAllocationGranularity = (64 * 1024) - 1;
  uintptr_t ptr_int = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t ret_ptr =
      (ptr_int + increment + kAllocationGranularity) & ~kAllocationGranularity;
  // Check for overflow.
  if (ret_ptr < ptr_int)
    return nullptr;
  return reinterpret_cast<char*>(ret_ptr);
}

// Allocate a memory block somewhere within 2GiB of a specified base address.
// This is used for the DLL hooking code to get a valid trampoline location
// which must be within +/- 2GiB of the base. We only consider +2GiB for now.
void* AllocateNearTo(void* source, size_t size) {
  using sandbox::g_nt;
  // 2GiB, maximum upper bound the allocation address must be within.
  const size_t kMaxSize = 0x80000000ULL;
  // We don't support null as a base as this would just pick an arbitrary
  // address when passed to NtAllocateVirtualMemory.
  if (!source)
    return nullptr;
  // Ignore an allocation which is larger than the maximum.
  if (size > kMaxSize)
    return nullptr;

  // Ensure base address is aligned to the allocation granularity boundary.
  char* base = AlignToBoundary(source, 0);
  if (!base)
    return nullptr;
  // Set top address to be base + 2GiB.
  const char* top_address = base + kMaxSize;

  while (base < top_address) {
    MEMORY_BASIC_INFORMATION mem_info;
    NTSTATUS status =
        g_nt.QueryVirtualMemory(NtCurrentProcess, base, MemoryBasicInformation,
                                &mem_info, sizeof(mem_info), nullptr);
    if (!NT_SUCCESS(status))
      break;

    if ((mem_info.State == MEM_FREE) && (mem_info.RegionSize >= size)) {
      // We've found a valid free block, try and allocate it for use.
      // Note that we need to both commit and reserve the block for the
      // allocation to succeed as per Windows virtual memory requirements.
      void* ret_base = mem_info.BaseAddress;
      status =
          g_nt.AllocateVirtualMemory(NtCurrentProcess, &ret_base, 0, &size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      // Shouldn't fail, but if it does we'll just continue and try next block.
      if (NT_SUCCESS(status))
        return ret_base;
    }

    // Update base past current allocation region.
    base = AlignToBoundary(mem_info.BaseAddress, mem_info.RegionSize);
    if (!base)
      break;
  }
  return nullptr;
}
#else   // defined(_WIN64).
void* AllocateNearTo(void* source, size_t size) {
  using sandbox::g_nt;

  // In 32-bit processes allocations below 512k are predictable, so mark
  // anything in that range as reserved and retry until we get a good address.
  const void* const kMinAddress = reinterpret_cast<void*>(512 * 1024);
  NTSTATUS ret;
  SIZE_T actual_size;
  void* base;
  do {
    base = nullptr;
    actual_size = 64 * 1024;
    ret = g_nt.AllocateVirtualMemory(NtCurrentProcess, &base, 0, &actual_size,
                                     MEM_RESERVE, PAGE_NOACCESS);
    if (!NT_SUCCESS(ret))
      return nullptr;
  } while (base < kMinAddress);

  actual_size = size;
  ret = g_nt.AllocateVirtualMemory(NtCurrentProcess, &base, 0, &actual_size,
                                   MEM_COMMIT, PAGE_READWRITE);
  if (!NT_SUCCESS(ret))
    return nullptr;
  return base;
}
#endif  // defined(_WIN64).

}  // namespace.

namespace sandbox {

// Handle for our private heap.
void* g_heap = nullptr;

SANDBOX_INTERCEPT HANDLE g_shared_section;
SANDBOX_INTERCEPT size_t g_shared_IPC_size = 0;
SANDBOX_INTERCEPT size_t g_shared_policy_size = 0;

void* volatile g_shared_policy_memory = nullptr;
void* volatile g_shared_IPC_memory = nullptr;

// Both the IPC and the policy share a single region of memory in which the IPC
// memory is first and the policy memory is last.
bool MapGlobalMemory() {
  if (!g_shared_IPC_memory) {
    void* memory = nullptr;
    SIZE_T size = 0;
    // Map the entire shared section from the start.
    NTSTATUS ret =
        g_nt.MapViewOfSection(g_shared_section, NtCurrentProcess, &memory, 0, 0,
                              nullptr, &size, ViewUnmap, 0, PAGE_READWRITE);

    if (!NT_SUCCESS(ret) || !memory) {
      NOTREACHED_NT();
      return false;
    }

    if (_InterlockedCompareExchangePointer(&g_shared_IPC_memory, memory,
                                           nullptr)) {
      // Somebody beat us to the memory setup.
      VERIFY_SUCCESS(g_nt.UnmapViewOfSection(NtCurrentProcess, memory));
    }
    DCHECK_NT(g_shared_IPC_size > 0);
    g_shared_policy_memory =
        reinterpret_cast<char*>(g_shared_IPC_memory) + g_shared_IPC_size;
  }
  DCHECK_NT(g_shared_policy_memory);
  DCHECK_NT(g_shared_policy_size > 0);
  return true;
}

void* GetGlobalIPCMemory() {
  if (!MapGlobalMemory())
    return nullptr;
  return g_shared_IPC_memory;
}

void* GetGlobalPolicyMemory() {
  if (!MapGlobalMemory())
    return nullptr;
  return g_shared_policy_memory;
}

bool InitHeap() {
  if (!g_heap) {
    // Create a new heap using default values for everything.
    void* heap =
        g_nt.RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0, nullptr, nullptr);
    if (!heap)
      return false;

    if (_InterlockedCompareExchangePointer(&g_heap, heap, nullptr)) {
      // Somebody beat us to the memory setup.
      g_nt.RtlDestroyHeap(heap);
    }
  }
  return !!g_heap;
}

// Physically reads or writes from memory to verify that (at this time), it is
// valid. Returns a dummy value.
int TouchMemory(void* buffer, size_t size_bytes, RequiredAccess intent) {
  const int kPageSize = 4096;
  int dummy = 0;
  volatile char* start = reinterpret_cast<char*>(buffer);
  volatile char* end = start + size_bytes - 1;

  if (WRITE == intent) {
    for (; start < end; start += kPageSize) {
      *start = *start;
    }
    *end = *end;
  } else {
    for (; start < end; start += kPageSize) {
      dummy += *start;
    }
    dummy += *end;
  }

  return dummy;
}

bool ValidParameter(void* buffer, size_t size, RequiredAccess intent) {
  DCHECK_NT(size);
  __try {
    TouchMemory(buffer, size, intent);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

NTSTATUS CopyData(void* destination, const void* source, size_t bytes) {
  NTSTATUS ret = STATUS_SUCCESS;
  __try {
    g_nt.memcpy(destination, source, bytes);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    ret = GetExceptionCode();
  }
  return ret;
}

NTSTATUS AllocAndGetFullPath(
    HANDLE root,
    const wchar_t* path,
    std::unique_ptr<wchar_t, NtAllocDeleter>* full_path) {
  if (!InitHeap())
    return STATUS_NO_MEMORY;

  DCHECK_NT(full_path);
  DCHECK_NT(path);
  NTSTATUS ret = STATUS_UNSUCCESSFUL;
  __try {
    do {
      static NtQueryObjectFunction NtQueryObject = nullptr;
      if (!NtQueryObject)
        ResolveNTFunctionPtr("NtQueryObject", &NtQueryObject);

      ULONG size = 0;
      // Query the name information a first time to get the size of the name.
      ret = NtQueryObject(root, ObjectNameInformation, nullptr, 0, &size);

      std::unique_ptr<OBJECT_NAME_INFORMATION, NtAllocDeleter> handle_name;
      if (size) {
        handle_name.reset(reinterpret_cast<OBJECT_NAME_INFORMATION*>(
            new (NT_ALLOC) BYTE[size]));

        // Query the name information a second time to get the name of the
        // object referenced by the handle.
        ret = NtQueryObject(root, ObjectNameInformation, handle_name.get(),
                            size, &size);
      }

      if (STATUS_SUCCESS != ret)
        break;

      // Space for path + '\' + name + '\0'.
      size_t name_length =
          handle_name->ObjectName.Length + (wcslen(path) + 2) * sizeof(wchar_t);
      full_path->reset(new (NT_ALLOC) wchar_t[name_length / sizeof(wchar_t)]);
      if (!*full_path)
        break;
      wchar_t* off = full_path->get();
      ret = CopyData(off, handle_name->ObjectName.Buffer,
                     handle_name->ObjectName.Length);
      if (!NT_SUCCESS(ret))
        break;
      off += handle_name->ObjectName.Length / sizeof(wchar_t);
      *off = L'\\';
      off += 1;
      ret = CopyData(off, path, wcslen(path) * sizeof(wchar_t));
      if (!NT_SUCCESS(ret))
        break;
      off += wcslen(path);
      *off = L'\0';
    } while (false);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    ret = GetExceptionCode();
  }

  if (!NT_SUCCESS(ret) && *full_path)
    full_path->reset(nullptr);

  return ret;
}

// Hacky code... replace with AllocAndCopyObjectAttributes.
NTSTATUS AllocAndCopyName(const OBJECT_ATTRIBUTES* in_object,
                          std::unique_ptr<wchar_t, NtAllocDeleter>* out_name,
                          uint32_t* attributes,
                          HANDLE* root) {
  if (!InitHeap())
    return STATUS_NO_MEMORY;

  DCHECK_NT(out_name);
  NTSTATUS ret = STATUS_UNSUCCESSFUL;
  __try {
    do {
      if (in_object->RootDirectory != static_cast<HANDLE>(0) && !root)
        break;
      if (!in_object->ObjectName)
        break;
      if (!in_object->ObjectName->Buffer)
        break;

      size_t size = in_object->ObjectName->Length + sizeof(wchar_t);
      out_name->reset(new (NT_ALLOC) wchar_t[size / sizeof(wchar_t)]);
      if (!*out_name)
        break;

      ret = CopyData(out_name->get(), in_object->ObjectName->Buffer,
                     size - sizeof(wchar_t));
      if (!NT_SUCCESS(ret))
        break;

      out_name->get()[size / sizeof(wchar_t) - 1] = L'\0';

      if (attributes)
        *attributes = in_object->Attributes;

      if (root)
        *root = in_object->RootDirectory;
      ret = STATUS_SUCCESS;
    } while (false);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    ret = GetExceptionCode();
  }

  if (!NT_SUCCESS(ret) && *out_name)
    out_name->reset(nullptr);

  return ret;
}

NTSTATUS GetProcessId(HANDLE process, DWORD* process_id) {
  PROCESS_BASIC_INFORMATION proc_info;
  ULONG bytes_returned;

  NTSTATUS ret =
      g_nt.QueryInformationProcess(process, ProcessBasicInformation, &proc_info,
                                   sizeof(proc_info), &bytes_returned);
  if (!NT_SUCCESS(ret) || sizeof(proc_info) != bytes_returned)
    return ret;

  *process_id = proc_info.UniqueProcessId;
  return STATUS_SUCCESS;
}

bool IsSameProcess(HANDLE process) {
  if (NtCurrentProcess == process)
    return true;

  static DWORD s_process_id = 0;

  if (!s_process_id) {
    NTSTATUS ret = GetProcessId(NtCurrentProcess, &s_process_id);
    if (!NT_SUCCESS(ret))
      return false;
  }

  DWORD process_id;
  NTSTATUS ret = GetProcessId(process, &process_id);
  if (!NT_SUCCESS(ret))
    return false;

  return (process_id == s_process_id);
}

bool IsValidImageSection(HANDLE section,
                         PVOID* base,
                         PLARGE_INTEGER offset,
                         PSIZE_T view_size) {
  if (!section || !base || !view_size || offset)
    return false;

  HANDLE query_section;

  NTSTATUS ret =
      g_nt.DuplicateObject(NtCurrentProcess, section, NtCurrentProcess,
                           &query_section, SECTION_QUERY, 0, 0);
  if (!NT_SUCCESS(ret))
    return false;

  SECTION_BASIC_INFORMATION basic_info;
  SIZE_T bytes_returned;
  ret = g_nt.QuerySection(query_section, SectionBasicInformation, &basic_info,
                          sizeof(basic_info), &bytes_returned);

  VERIFY_SUCCESS(g_nt.Close(query_section));

  if (!NT_SUCCESS(ret) || sizeof(basic_info) != bytes_returned)
    return false;

  if (!(basic_info.Attributes & SEC_IMAGE))
    return false;

  return true;
}

UNICODE_STRING* AnsiToUnicode(const char* string) {
  ANSI_STRING ansi_string;
  ansi_string.Length = static_cast<USHORT>(g_nt.strlen(string));
  ansi_string.MaximumLength = ansi_string.Length + 1;
  ansi_string.Buffer = const_cast<char*>(string);

  if (ansi_string.Length > ansi_string.MaximumLength)
    return nullptr;

  size_t name_bytes =
      ansi_string.MaximumLength * sizeof(wchar_t) + sizeof(UNICODE_STRING);

  UNICODE_STRING* out_string =
      reinterpret_cast<UNICODE_STRING*>(new (NT_ALLOC) char[name_bytes]);
  if (!out_string)
    return nullptr;

  out_string->MaximumLength = ansi_string.MaximumLength * sizeof(wchar_t);
  out_string->Buffer = reinterpret_cast<wchar_t*>(&out_string[1]);

  BOOLEAN alloc_destination = false;
  NTSTATUS ret = g_nt.RtlAnsiStringToUnicodeString(out_string, &ansi_string,
                                                   alloc_destination);
  DCHECK_NT(STATUS_BUFFER_OVERFLOW != ret);
  if (!NT_SUCCESS(ret)) {
    operator delete(out_string, NT_ALLOC);
    return nullptr;
  }

  return out_string;
}

UNICODE_STRING* GetImageInfoFromModule(HMODULE module, uint32_t* flags) {
// PEImage's dtor won't be run during SEH unwinding, but that's OK.
#pragma warning(push)
#pragma warning(disable : 4509)
  UNICODE_STRING* out_name = nullptr;
  __try {
    do {
      *flags = 0;
      base::win::PEImage pe(module);

      if (!pe.VerifyMagic())
        break;
      *flags |= MODULE_IS_PE_IMAGE;

      PIMAGE_EXPORT_DIRECTORY exports = pe.GetExportDirectory();
      if (exports) {
        char* name = reinterpret_cast<char*>(pe.RVAToAddr(exports->Name));
        out_name = AnsiToUnicode(name);
      }

      PIMAGE_NT_HEADERS headers = pe.GetNTHeaders();
      if (headers) {
        if (headers->OptionalHeader.AddressOfEntryPoint)
          *flags |= MODULE_HAS_ENTRY_POINT;
        if (headers->OptionalHeader.SizeOfCode)
          *flags |= MODULE_HAS_CODE;
      }
    } while (false);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return out_name;
#pragma warning(pop)
}

const char* GetAnsiImageInfoFromModule(HMODULE module) {
// PEImage's dtor won't be run during SEH unwinding, but that's OK.
#pragma warning(push)
#pragma warning(disable : 4509)
  const char* out_name = nullptr;
  __try {
    do {
      base::win::PEImage pe(module);

      if (!pe.VerifyMagic())
        break;

      PIMAGE_EXPORT_DIRECTORY exports = pe.GetExportDirectory();
      if (exports)
        out_name = static_cast<const char*>(pe.RVAToAddr(exports->Name));
    } while (false);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return out_name;
#pragma warning(pop)
}

UNICODE_STRING* GetBackingFilePath(PVOID address) {
  // We'll start with something close to max_path charactes for the name.
  SIZE_T buffer_bytes = MAX_PATH * 2;

  for (;;) {
    MEMORY_SECTION_NAME* section_name = reinterpret_cast<MEMORY_SECTION_NAME*>(
        new (NT_ALLOC) char[buffer_bytes]);

    if (!section_name)
      return nullptr;

    SIZE_T returned_bytes;
    NTSTATUS ret =
        g_nt.QueryVirtualMemory(NtCurrentProcess, address, MemorySectionName,
                                section_name, buffer_bytes, &returned_bytes);

    if (STATUS_BUFFER_OVERFLOW == ret) {
      // Retry the call with the given buffer size.
      operator delete(section_name, NT_ALLOC);
      section_name = nullptr;
      buffer_bytes = returned_bytes;
      continue;
    }
    if (!NT_SUCCESS(ret)) {
      operator delete(section_name, NT_ALLOC);
      return nullptr;
    }

    return reinterpret_cast<UNICODE_STRING*>(section_name);
  }
}

UNICODE_STRING* ExtractModuleName(const UNICODE_STRING* module_path) {
  if ((!module_path) || (!module_path->Buffer))
    return nullptr;

  wchar_t* sep = nullptr;
  int start_pos = module_path->Length / sizeof(wchar_t) - 1;
  int ix = start_pos;

  for (; ix >= 0; --ix) {
    if (module_path->Buffer[ix] == L'\\') {
      sep = &module_path->Buffer[ix];
      break;
    }
  }

  // Ends with path separator. Not a valid module name.
  if ((ix == start_pos) && sep)
    return nullptr;

  // No path separator found. Use the entire name.
  if (!sep) {
    sep = &module_path->Buffer[-1];
  }

  // Add one to the size so we can null terminate the string.
  size_t size_bytes = (start_pos - ix + 1) * sizeof(wchar_t);

  // Based on the code above, size_bytes should always be small enough
  // to make the static_cast below safe.
  DCHECK_NT(UINT16_MAX > size_bytes);
  char* str_buffer = new (NT_ALLOC) char[size_bytes + sizeof(UNICODE_STRING)];
  if (!str_buffer)
    return nullptr;

  UNICODE_STRING* out_string = reinterpret_cast<UNICODE_STRING*>(str_buffer);
  out_string->Buffer = reinterpret_cast<wchar_t*>(&out_string[1]);
  out_string->Length = static_cast<USHORT>(size_bytes - sizeof(wchar_t));
  out_string->MaximumLength = static_cast<USHORT>(size_bytes);

  NTSTATUS ret = CopyData(out_string->Buffer, &sep[1], out_string->Length);
  if (!NT_SUCCESS(ret)) {
    operator delete(out_string, NT_ALLOC);
    return nullptr;
  }

  out_string->Buffer[out_string->Length / sizeof(wchar_t)] = L'\0';
  return out_string;
}

NTSTATUS AutoProtectMemory::ChangeProtection(void* address,
                                             size_t bytes,
                                             ULONG protect) {
  DCHECK_NT(!changed_);
  SIZE_T new_bytes = bytes;
  NTSTATUS ret = g_nt.ProtectVirtualMemory(NtCurrentProcess, &address,
                                           &new_bytes, protect, &old_protect_);
  if (NT_SUCCESS(ret)) {
    changed_ = true;
    address_ = address;
    bytes_ = new_bytes;
  }

  return ret;
}

NTSTATUS AutoProtectMemory::RevertProtection() {
  if (!changed_)
    return STATUS_SUCCESS;

  DCHECK_NT(address_);
  DCHECK_NT(bytes_);

  SIZE_T new_bytes = bytes_;
  NTSTATUS ret = g_nt.ProtectVirtualMemory(
      NtCurrentProcess, &address_, &new_bytes, old_protect_, &old_protect_);
  DCHECK_NT(NT_SUCCESS(ret));

  changed_ = false;
  address_ = nullptr;
  bytes_ = 0;
  old_protect_ = 0;

  return ret;
}

bool IsSupportedRenameCall(FILE_RENAME_INFORMATION* file_info,
                           DWORD length,
                           uint32_t file_info_class) {
  if (FileRenameInformation != file_info_class)
    return false;

  if (length < sizeof(FILE_RENAME_INFORMATION))
    return false;

  // Make sure file name length doesn't exceed the message length
  if (length - offsetof(FILE_RENAME_INFORMATION, FileName) <
      file_info->FileNameLength)
    return false;

  // We don't support a root directory.
  if (file_info->RootDirectory)
    return false;

  static const wchar_t kPathPrefix[] = {L'\\', L'?', L'?', L'\\'};

  // Check if it starts with \\??\\. We don't support relative paths.
  if (file_info->FileNameLength < sizeof(kPathPrefix) ||
      file_info->FileNameLength > UINT16_MAX)
    return false;

  if (file_info->FileName[0] != kPathPrefix[0] ||
      file_info->FileName[1] != kPathPrefix[1] ||
      file_info->FileName[2] != kPathPrefix[2] ||
      file_info->FileName[3] != kPathPrefix[3])
    return false;

  return true;
}

bool NtGetPathFromHandle(HANDLE handle,
                         std::unique_ptr<wchar_t, NtAllocDeleter>* path) {
  OBJECT_NAME_INFORMATION initial_buffer;
  OBJECT_NAME_INFORMATION* name;
  ULONG size = 0;
  // Query the name information a first time to get the size of the name.
  NTSTATUS status = g_nt.QueryObject(handle, ObjectNameInformation,
                                     &initial_buffer, size, &size);

  if (!NT_SUCCESS(status) && status != STATUS_INFO_LENGTH_MISMATCH)
    return false;

  std::unique_ptr<BYTE[], NtAllocDeleter> name_ptr;
  if (!size)
    return false;
  name_ptr.reset(new (NT_ALLOC) BYTE[size]);
  name = reinterpret_cast<OBJECT_NAME_INFORMATION*>(name_ptr.get());

  // Query the name information a second time to get the name of the
  // object referenced by the handle.
  status = g_nt.QueryObject(handle, ObjectNameInformation, name, size, &size);

  if (STATUS_SUCCESS != status)
    return false;
  size_t num_path_wchars = (name->ObjectName.Length / sizeof(wchar_t)) + 1;
  path->reset(new (NT_ALLOC) wchar_t[num_path_wchars]);
  status =
      CopyData(path->get(), name->ObjectName.Buffer, name->ObjectName.Length);
  path->get()[num_path_wchars - 1] = L'\0';
  if (STATUS_SUCCESS != status)
    return false;
  return true;
}

}  // namespace sandbox

void* operator new(size_t size, sandbox::AllocationType type, void* near_to) {
  void* result = nullptr;
  if (type == sandbox::NT_ALLOC) {
    if (sandbox::InitHeap()) {
      // Use default flags for the allocation.
      result = sandbox::g_nt.RtlAllocateHeap(sandbox::g_heap, 0, size);
    }
  } else if (type == sandbox::NT_PAGE) {
    result = AllocateNearTo(near_to, size);
  } else {
    NOTREACHED_NT();
  }

  // TODO: Returning nullptr from operator new has undefined behavior, but
  // the Allocate() functions called above can return nullptr. Consider checking
  // for nullptr here and crashing or throwing.

  return result;
}

void operator delete(void* memory, sandbox::AllocationType type) {
  if (type == sandbox::NT_ALLOC) {
    // Use default flags.
    VERIFY(sandbox::g_nt.RtlFreeHeap(sandbox::g_heap, 0, memory));
  } else if (type == sandbox::NT_PAGE) {
    void* base = memory;
    SIZE_T size = 0;
    VERIFY_SUCCESS(sandbox::g_nt.FreeVirtualMemory(NtCurrentProcess, &base,
                                                   &size, MEM_RELEASE));
  } else {
    NOTREACHED_NT();
  }
}

void operator delete(void* memory,
                     sandbox::AllocationType type,
                     void* near_to) {
  operator delete(memory, type);
}

void* __cdecl operator new(size_t size,
                           void* buffer,
                           sandbox::AllocationType type) {
  return buffer;
}

void __cdecl operator delete(void* memory,
                             void* buffer,
                             sandbox::AllocationType type) {}
