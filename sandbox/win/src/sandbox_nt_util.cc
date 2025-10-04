// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/sandbox_nt_util.h"

#include <ntstatus.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/win/pe_image.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt;

}  // namespace sandbox

namespace {

// Uses value of FILE_INFORMATION_CLASS defined in Wdm.h but not in user-mode.
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ne-wdm-_file_information_class
constexpr uint32_t FileRenameInformation = 10;

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
    // Avoid memset inserted by -ftrivial-auto-var-init=pattern.
    STACK_UNINITIALIZED MEMORY_BASIC_INFORMATION mem_info;
    NTSTATUS status = sandbox::GetNtExports()->QueryVirtualMemory(
        NtCurrentProcess, base, MemoryBasicInformation, &mem_info,
        sizeof(mem_info), nullptr);
    if (!NT_SUCCESS(status))
      break;

    if ((mem_info.State == MEM_FREE) && (mem_info.RegionSize >= size)) {
      // We've found a valid free block, try and allocate it for use.
      // Note that we need to both commit and reserve the block for the
      // allocation to succeed as per Windows virtual memory requirements.
      void* ret_base = mem_info.BaseAddress;
      status = sandbox::GetNtExports()->AllocateVirtualMemory(
          NtCurrentProcess, &ret_base, 0, &size, MEM_COMMIT | MEM_RESERVE,
          PAGE_READWRITE);
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
  // In 32-bit processes allocations below 512k are predictable, so mark
  // anything in that range as reserved and retry until we get a good address.
  const void* const kMinAddress = reinterpret_cast<void*>(512 * 1024);
  NTSTATUS ret;
  SIZE_T actual_size;
  void* base;
  do {
    base = nullptr;
    actual_size = 64 * 1024;
    ret = sandbox::GetNtExports()->AllocateVirtualMemory(
        NtCurrentProcess, &base, 0, &actual_size, MEM_RESERVE, PAGE_NOACCESS);
    if (!NT_SUCCESS(ret))
      return nullptr;
  } while (base < kMinAddress);

  actual_size = size;
  ret = sandbox::GetNtExports()->AllocateVirtualMemory(
      NtCurrentProcess, &base, 0, &actual_size, MEM_COMMIT, PAGE_READWRITE);
  if (!NT_SUCCESS(ret))
    return nullptr;
  return base;
}
#endif  // defined(_WIN64).

template <typename T>
void InitFunc(const base::win::PEImage& image, T& member, const char* name) {
  member = reinterpret_cast<T>(image.GetProcAddress(name));
  DCHECK(member);
}

#define INIT_NT(member) InitFunc(image, sandbox::g_nt.member, "Nt" #member)
#define INIT_RTL(member) InitFunc(image, sandbox::g_nt.member, #member)

void InitGlobalNt() {
  HMODULE ntdll = ::GetModuleHandle(sandbox::kNtdllName);
  base::win::PEImage image(ntdll);
  INIT_NT(AllocateVirtualMemory);
  INIT_NT(CreateFile);
  INIT_NT(CreateSection);
  INIT_NT(Close);
  INIT_NT(DuplicateObject);
  INIT_NT(FreeVirtualMemory);
  INIT_NT(MapViewOfSection);
  INIT_NT(OpenThread);
  INIT_NT(OpenProcessTokenEx);
  INIT_NT(ProtectVirtualMemory);
  INIT_NT(QueryAttributesFile);
  INIT_NT(QueryFullAttributesFile);
  INIT_NT(QueryInformationProcess);
  INIT_NT(QueryObject);
  INIT_NT(QuerySection);
  INIT_NT(QueryVirtualMemory);
  INIT_NT(SetInformationFile);
  INIT_NT(SignalAndWaitForSingleObject);
  INIT_NT(UnmapViewOfSection);
  INIT_NT(WaitForSingleObject);
  INIT_RTL(RtlAllocateHeap);
  INIT_RTL(RtlAnsiStringToUnicodeString);
  INIT_RTL(RtlInitAnsiString);
  INIT_RTL(RtlCompareUnicodeString);
  INIT_RTL(RtlCreateHeap);
  INIT_RTL(RtlDestroyHeap);
  INIT_RTL(RtlFreeHeap);
  INIT_RTL(RtlNtStatusToDosError);
  INIT_RTL(_strnicmp);
  INIT_RTL(memcpy);
  sandbox::g_nt.Initialized = true;
}

// The TEB structure defined in winternl.h doesn't have the ClientId member.
// Provide a partial definition here.
struct PARTIAL_TEB {
  PVOID NtTib[7];
  PVOID EnvironmentPointer;
  CLIENT_ID ClientId;
  PVOID ActiveRpcHandle;
  PVOID ThreadLocalStoragePointer;
  PPEB ProcessEnvironmentBlock;
};

// Check PEB offset between the partial definition and the public one.
static_assert(offsetof(PARTIAL_TEB, ProcessEnvironmentBlock) ==
              offsetof(TEB, ProcessEnvironmentBlock));

// Can't use the base::win version in component builds so just repeat it.
bool ViewToUnicodeString(std::wstring_view str, UNICODE_STRING& ustr) {
  constexpr size_t kMaxLength = UINT16_MAX / sizeof(WCHAR);
  if (std::size(str) > kMaxLength) {
    return false;
  }
  ustr.Buffer = const_cast<WCHAR*>(str.data());
  ustr.Length = static_cast<USHORT>(std::size(str) * sizeof(WCHAR));
  ustr.MaximumLength = ustr.Length;
  return true;
}

}  // namespace.

namespace sandbox {

// Handle for our private heap.
void* g_heap = nullptr;

SANDBOX_INTERCEPT HANDLE g_shared_section;
SANDBOX_INTERCEPT size_t g_shared_IPC_size = 0;
SANDBOX_INTERCEPT size_t g_shared_policy_size = 0;
SANDBOX_INTERCEPT size_t g_delegate_data_size = 0;

void* volatile g_shared_policy_memory = nullptr;
void* volatile g_shared_IPC_memory = nullptr;
void* volatile g_shared_delegate_data = nullptr;

// The IPC, policy and delegate data share a single region of memory with blocks
// in that order.
bool MapGlobalMemory() {
  if (!g_shared_IPC_memory) {
    void* memory = nullptr;
    SIZE_T size = 0;
    // Map the entire shared section from the start.
    NTSTATUS ret = GetNtExports()->MapViewOfSection(
        g_shared_section, NtCurrentProcess, &memory, 0, 0, nullptr, &size,
        ViewUnmap, 0, PAGE_READWRITE);

    if (!NT_SUCCESS(ret) || !memory) {
      NOTREACHED_NT();
      return false;
    }

    if (_InterlockedCompareExchangePointer(&g_shared_IPC_memory, memory,
                                           nullptr)) {
      // Somebody beat us to the memory setup.
      VERIFY_SUCCESS(
          GetNtExports()->UnmapViewOfSection(NtCurrentProcess, memory));
    }
    DCHECK_NT(g_shared_IPC_size > 0);

    if (g_shared_policy_size > 0) {
      g_shared_policy_memory =
          reinterpret_cast<char*>(g_shared_IPC_memory) + g_shared_IPC_size;
    }
    // TODO(crbug.com/40265190) make this a read-only mapping in the child,
    // distinct from the IPC & policy memory as it should be const.
    if (g_delegate_data_size > 0) {
      g_shared_delegate_data = reinterpret_cast<char*>(g_shared_IPC_memory) +
                               g_shared_IPC_size + g_shared_policy_size;
    }
  }

  return true;
}

ScopedUnicodeString::ScopedUnicodeString() = default;
ScopedUnicodeString::~ScopedUnicodeString() = default;
ScopedUnicodeString::ScopedUnicodeString(
    std::unique_ptr<uint8_t, NtAllocDeleter> buffer,
    std::wstring_view str)
    : buffer_(std::move(buffer)), str_(str) {}

void* GetGlobalIPCMemory() {
  if (!MapGlobalMemory())
    return nullptr;
  return g_shared_IPC_memory;
}

void* GetGlobalPolicyMemoryForTesting() {
  if (!MapGlobalMemory())
    return nullptr;
  return g_shared_policy_memory;
}

std::optional<base::span<const uint8_t>> GetGlobalDelegateData() {
  if (!g_delegate_data_size) {
    return std::nullopt;
  }
  if (!MapGlobalMemory()) {
    return std::nullopt;
  }
  return base::span(reinterpret_cast<const uint8_t*>(g_shared_delegate_data),
                    g_delegate_data_size);
}

const NtExports* GetNtExports() {
  if (!g_nt.Initialized)
    InitGlobalNt();

  return &g_nt;
}

bool InitHeap() {
  if (!g_heap) {
    // Create a new heap using default values for everything.
    void* heap = GetNtExports()->RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0,
                                               nullptr, nullptr);
    if (!heap)
      return false;

    if (_InterlockedCompareExchangePointer(&g_heap, heap, nullptr)) {
      // Somebody beat us to the memory setup.
      GetNtExports()->RtlDestroyHeap(heap);
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

NTSTATUS GetProcessId(HANDLE process, DWORD* process_id) {
  PROCESS_BASIC_INFORMATION proc_info;
  ULONG bytes_returned;

  NTSTATUS ret = GetNtExports()->QueryInformationProcess(
      process, ProcessBasicInformation, &proc_info, sizeof(proc_info),
      &bytes_returned);
  if (!NT_SUCCESS(ret) || sizeof(proc_info) != bytes_returned)
    return ret;

  // https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess
  // "UniqueProcessId Can be cast to a DWORD and contains a unique identifier
  // for this process."
  *process_id = static_cast<DWORD>(proc_info.UniqueProcessId);
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

  NTSTATUS ret = GetNtExports()->DuplicateObject(
      NtCurrentProcess, section, NtCurrentProcess, &query_section,
      SECTION_QUERY, 0, 0);
  if (!NT_SUCCESS(ret))
    return false;

  SECTION_BASIC_INFORMATION basic_info;
  SIZE_T bytes_returned;
  ret = GetNtExports()->QuerySection(query_section, SectionBasicInformation,
                                     &basic_info, sizeof(basic_info),
                                     &bytes_returned);

  VERIFY_SUCCESS(GetNtExports()->Close(query_section));

  if (!NT_SUCCESS(ret) || sizeof(basic_info) != bytes_returned)
    return false;

  if (!(basic_info.Attributes & SEC_IMAGE))
    return false;

  // Windows 10 2009+ may open PEs as SEC_IMAGE_NO_EXECUTE in non-dll-loading
  // paths which looks identical to dll-loading unless we check if the section
  // handle has execute rights.
  // Avoid memset inserted by -ftrivial-auto-var-init=pattern.
  STACK_UNINITIALIZED OBJECT_BASIC_INFORMATION obj_info;
  ULONG obj_size_returned;
  ret = GetNtExports()->QueryObject(section, ObjectBasicInformation, &obj_info,
                                    sizeof(obj_info), &obj_size_returned);

  if (!NT_SUCCESS(ret) || sizeof(obj_info) != obj_size_returned)
    return false;

  if (!(obj_info.GrantedAccess & SECTION_MAP_EXECUTE))
    return false;

  return true;
}

ScopedUnicodeString AnsiToUnicode(const char* string) {
  STACK_UNINITIALIZED ANSI_STRING ansi_string;
  GetNtExports()->RtlInitAnsiString(&ansi_string, string);

  size_t name_bytes = ansi_string.MaximumLength * sizeof(wchar_t);

  auto buffer = std::unique_ptr<uint8_t, NtAllocDeleter>(
      new (NT_ALLOC) uint8_t[name_bytes]);
  if (!buffer) {
    return {};
  }

  STACK_UNINITIALIZED UNICODE_STRING out_string;
  out_string.Length = 0;
  out_string.MaximumLength = ansi_string.MaximumLength * sizeof(wchar_t);
  out_string.Buffer = reinterpret_cast<wchar_t*>(buffer.get());

  NTSTATUS ret = GetNtExports()->RtlAnsiStringToUnicodeString(
      &out_string, &ansi_string, FALSE);
  DCHECK_NT(STATUS_BUFFER_OVERFLOW != ret);
  if (!NT_SUCCESS(ret)) {
    return {};
  }

  return {std::move(buffer),
          std::wstring_view(out_string.Buffer,
                            out_string.Length / sizeof(wchar_t))};
}

ScopedUnicodeString GetImageInfoFromModule(HMODULE module, bool* has_code) {
// PEImage's dtor won't be run during SEH unwinding, but that's OK.
#pragma warning(push)
#pragma warning(disable : 4509)
  __try {
    *has_code = false;
    base::win::PEImage pe(module);

    if (!pe.VerifyMagic()) {
      return {};
    }
    PIMAGE_NT_HEADERS headers = pe.GetNTHeaders();
    *has_code = headers && !!headers->OptionalHeader.SizeOfCode;

    PIMAGE_EXPORT_DIRECTORY exports = pe.GetExportDirectory();
    if (exports) {
      char* name = reinterpret_cast<char*>(pe.RVAToAddr(exports->Name));
      return AnsiToUnicode(name);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return {};
#pragma warning(pop)
}

const char* GetAnsiImageInfoFromModule(HMODULE module) {
// PEImage's dtor won't be run during SEH unwinding, but that's OK.
#pragma warning(push)
#pragma warning(disable : 4509)
  __try {
      base::win::PEImage pe(module);

      if (!pe.VerifyMagic()) {
        return nullptr;
      }

      PIMAGE_EXPORT_DIRECTORY exports = pe.GetExportDirectory();
      if (exports) {
        return static_cast<const char*>(pe.RVAToAddr(exports->Name));
      }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return nullptr;
#pragma warning(pop)
}

ScopedUnicodeString GetBackingFilePath(PVOID address) {
  // We'll start with something close to max_path charactes for the name.
  SIZE_T buffer_bytes = MAX_PATH * 2;

  for (;;) {
    std::unique_ptr<uint8_t, NtAllocDeleter> section_name(
        new (NT_ALLOC) uint8_t[buffer_bytes]);
    if (!section_name) {
      return {};
    }

    SIZE_T returned_bytes;
    NTSTATUS ret = GetNtExports()->QueryVirtualMemory(
        NtCurrentProcess, address, MemorySectionName, section_name.get(),
        buffer_bytes, &returned_bytes);

    if (STATUS_BUFFER_OVERFLOW == ret) {
      // Retry the call with the given buffer size.
      buffer_bytes = returned_bytes;
      continue;
    }
    if (!NT_SUCCESS(ret)) {
      return {};
    }

    UNICODE_STRING* str = reinterpret_cast<UNICODE_STRING*>(section_name.get());
    return {std::move(section_name),
            std::wstring_view(str->Buffer, str->Length / sizeof(wchar_t))};
  }
}

std::wstring_view ExtractModuleName(std::wstring_view module_path) {
  if (module_path.empty()) {
    return {};
  }
  auto last_slash = module_path.rfind(L'\\');
  // If the string ends with path separator then this will return an empty
  // string which signifies no module name.
  if (last_slash != std::wstring_view::npos) {
    module_path.remove_prefix(last_slash + 1);
  }
  return module_path;
}

std::optional<bool> EqualUnicodeString(std::wstring_view left,
                                       std::wstring_view right) {
  UNICODE_STRING left_ustr;
  UNICODE_STRING right_ustr;
  if (!ViewToUnicodeString(left, left_ustr) ||
      !ViewToUnicodeString(right, right_ustr)) {
    return std::nullopt;
  }

  return GetNtExports()->RtlCompareUnicodeString(&left_ustr, &right_ustr,
                                                 TRUE) == 0;
}

NTSTATUS AutoProtectMemory::ChangeProtection(void* address,
                                             size_t bytes,
                                             ULONG protect) {
  DCHECK_NT(!changed_);
  SIZE_T new_bytes = bytes;
  NTSTATUS ret = GetNtExports()->ProtectVirtualMemory(
      NtCurrentProcess, &address, &new_bytes, protect, &old_protect_);
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
  NTSTATUS ret = GetNtExports()->ProtectVirtualMemory(
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

CLIENT_ID GetCurrentClientId() {
  return reinterpret_cast<PARTIAL_TEB*>(NtCurrentTeb())->ClientId;
}

}  // namespace sandbox

void* operator new(size_t size, sandbox::AllocationType type, void* near_to) {
  void* result = nullptr;
  if (type == sandbox::NT_ALLOC) {
    if (sandbox::InitHeap()) {
      // Use default flags for the allocation.
      result =
          sandbox::GetNtExports()->RtlAllocateHeap(sandbox::g_heap, 0, size);
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
    VERIFY(sandbox::GetNtExports()->RtlFreeHeap(sandbox::g_heap, 0, memory));
  } else if (type == sandbox::NT_PAGE) {
    void* base = memory;
    SIZE_T size = 0;
    VERIFY_SUCCESS(sandbox::GetNtExports()->FreeVirtualMemory(
        NtCurrentProcess, &base, &size, MEM_RELEASE));
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
