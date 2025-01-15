// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SANDBOX_WIN_SRC_SANDBOX_NT_UTIL_H_
#define SANDBOX_WIN_SRC_SANDBOX_NT_UTIL_H_

#include <intrin.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>

#include <optional>
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_types.h"

// Placement new and delete to be used from ntdll interception code.
void* __cdecl operator new(size_t size,
                           sandbox::AllocationType type,
                           void* near_to = nullptr);
void __cdecl operator delete(void* memory, sandbox::AllocationType type);
// Add operator delete that matches the placement form of the operator new
// above. This is required by compiler to generate code to call operator delete
// in case the object's constructor throws an exception.
// See http://msdn.microsoft.com/en-us/library/cxdxz3x6.aspx
void __cdecl operator delete(void* memory,
                             sandbox::AllocationType type,
                             void* near_to);

// Regular placement new and delete
void* __cdecl operator new(size_t size,
                           void* buffer,
                           sandbox::AllocationType type);
void __cdecl operator delete(void* memory,
                             void* buffer,
                             sandbox::AllocationType type);

// DCHECK_NT is defined to be pretty much an assert at this time because we
// don't have logging from the ntdll layer on the child.
//
// VERIFY_NT and VERIFY_SUCCESS are the standard asserts on debug, but
// execute the actual argument on release builds. VERIFY_NT expects an action
// returning a bool, while VERIFY_SUCCESS expects an action returning
// NTSTATUS.
#ifndef NDEBUG
#define DCHECK_NT(condition) \
  { (condition) ? (void)0 : __debugbreak(); }
#define VERIFY(action) DCHECK_NT(action)
#define VERIFY_SUCCESS(action) DCHECK_NT(NT_SUCCESS(action))
#else
#define DCHECK_NT(condition)
#define VERIFY(action) (action)
#define VERIFY_SUCCESS(action) (action)
#endif

#define CHECK_NT(condition) \
  { (condition) ? (void)0 : __debugbreak(); }

#define NOTREACHED_NT() DCHECK_NT(false)

namespace sandbox {

#if defined(_M_X64) || defined(_M_ARM64)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedCompareExchangePointer)

#elif defined(_M_IX86)
extern "C" long _InterlockedCompareExchange(long volatile* destination,
                                            long exchange,
                                            long comperand);

#pragma intrinsic(_InterlockedCompareExchange)

// We want to make sure that we use an intrinsic version of the function, not
// the one provided by kernel32.
__forceinline void* _InterlockedCompareExchangePointer(
    void* volatile* destination,
    void* exchange,
    void* comperand) {
  long ret = _InterlockedCompareExchange(
      reinterpret_cast<long volatile*>(destination),
      static_cast<long>(reinterpret_cast<size_t>(exchange)),
      static_cast<long>(reinterpret_cast<size_t>(comperand)));

  return reinterpret_cast<void*>(static_cast<size_t>(ret));
}

#else
#error Architecture not supported.

#endif

struct NtAllocDeleter {
  inline void operator()(void* ptr) const {
    operator delete(ptr, AllocationType::NT_ALLOC);
  }
};

// Returns a pointer to the IPC shared memory.
void* GetGlobalIPCMemory();

// Returns a pointer to the Policy shared memory.
void* GetGlobalPolicyMemoryForTesting();

// Returns a view of the shared delegate data, or nullopt if none was provided.
std::optional<base::span<const uint8_t>> GetGlobalDelegateData();

// Returns a reference to imported NT functions.
const NtExports* GetNtExports();

enum RequiredAccess { READ, WRITE };

// Performs basic user mode buffer validation. In any case, buffers access must
// be protected by SEH. intent specifies if the buffer should be tested for read
// or write.
bool ValidParameter(void* buffer, size_t size, RequiredAccess intent);

// Copies data from a user buffer to our buffer. Returns the operation status.
NTSTATUS CopyData(void* destination, const void* source, size_t bytes);

// Copies the name from an object attributes. |out_name| is a NUL terminated
// string and |out_name_len| is the number of characters copied. |attributes|
// is a copy of the attribute flags from |in_object|.
NTSTATUS CopyNameAndAttributes(
    const OBJECT_ATTRIBUTES* in_object,
    std::unique_ptr<wchar_t, NtAllocDeleter>* out_name,
    size_t* out_name_len,
    uint32_t* attributes = nullptr);

// Initializes our ntdll level heap
bool InitHeap();

// Returns true if the provided handle refers to the current process.
bool IsSameProcess(HANDLE process);

enum MappedModuleFlags {
  MODULE_IS_PE_IMAGE = 1,      // Module is an executable.
  MODULE_HAS_ENTRY_POINT = 2,  // Execution entry point found.
  MODULE_HAS_CODE = 4          // Non zero size of executable sections.
};

// Returns the name and characteristics for a given PE module. The return
// value is the name as defined by the export table and the flags is any
// combination of the MappedModuleFlags enumeration.
//
// The returned buffer must be freed with a placement delete from the ntdll
// level allocator:
//
// UNICODE_STRING* name = GetPEImageInfoFromModule(HMODULE module, &flags);
// if (!name) {
//   // probably not a valid dll
//   return;
// }
// InsertYourLogicHere(name);
// operator delete(name, NT_ALLOC);
UNICODE_STRING* GetImageInfoFromModule(HMODULE module, uint32_t* flags);

// Returns the name and characteristics for a given PE module. The return
// value is the name as defined by the export table.
//
// The returned buffer is within the PE module and must not be freed.
const char* GetAnsiImageInfoFromModule(HMODULE module);

// Returns the full path and filename for a given dll.
// May return nullptr if the provided address is not backed by a named section,
// or if the current OS version doesn't support the call. The returned buffer
// must be freed with a placement delete (see GetImageNameFromModule example).
UNICODE_STRING* GetBackingFilePath(PVOID address);

// Returns the last component of a path that contains the module name.
// It will return nullptr if the path ends with the path separator. The returned
// buffer must be freed with a placement delete (see GetImageNameFromModule
// example).
UNICODE_STRING* ExtractModuleName(const UNICODE_STRING* module_path);

// Returns true if the parameters correspond to a dll mapped as code.
bool IsValidImageSection(HANDLE section,
                         PVOID* base,
                         PLARGE_INTEGER offset,
                         PSIZE_T view_size);

// Converts an ansi string to an UNICODE_STRING.
UNICODE_STRING* AnsiToUnicode(const char* string);

// Provides a simple way to temporarily change the protection of a memory page.
class AutoProtectMemory {
 public:
  AutoProtectMemory()
      : changed_(false), address_(nullptr), bytes_(0), old_protect_(0) {}

  AutoProtectMemory(const AutoProtectMemory&) = delete;
  AutoProtectMemory& operator=(const AutoProtectMemory&) = delete;

  ~AutoProtectMemory() { RevertProtection(); }

  // Sets the desired protection of a given memory range.
  NTSTATUS ChangeProtection(void* address, size_t bytes, ULONG protect);

  // Restores the original page protection.
  NTSTATUS RevertProtection();

 private:
  bool changed_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION void* address_;
  size_t bytes_;
  ULONG old_protect_;
};

// Returns true if the file_rename_information structure is supported by our
// rename handler.
bool IsSupportedRenameCall(FILE_RENAME_INFORMATION* file_info,
                           DWORD length,
                           uint32_t file_info_class);

// Get the CLIENT_ID from the current TEB.
CLIENT_ID GetCurrentClientId();

// Version of memset that can be called before the CRT is initialized.
__forceinline void Memset(void* ptr, int value, size_t num_bytes) {
  unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
  while (num_bytes--) {
    *byte_ptr++ = static_cast<unsigned char>(value);
  }
}

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_NT_UTIL_H_
