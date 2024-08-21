// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/handle_closer_agent.h"

#include <Windows.h>
#include <winnls.h>

#include <stddef.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/win/static_constants.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/heap_helper.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {
namespace {
// Partial definition only for value not in PROCESSINFOCLASS.
constexpr uint32_t ProcessHandleTable = 58;

// Handle Types.
constexpr wchar_t kFile[] = L"File";
constexpr wchar_t kSection[] = L"Section";
constexpr wchar_t kALPCPort[] = L"ALPC Port";
// Full paths of closeable handles.
constexpr wchar_t kDeviceApi[] = L"\\Device\\DeviceApi";
constexpr wchar_t kDeviceKsecDD[] = L"\\Device\\KsecDD";
// Leaf name of Section to close (matches with EndsWith).
constexpr wchar_t kWindowsShellGlobalCounters[] =
    L"\\windows_shell_global_counters";

// Used by EnumSystemLocales for warming up.
static BOOL CALLBACK EnumLocalesProcEx(LPWSTR lpLocaleString,
                                       DWORD dwFlags,
                                       LPARAM lParam) {
  return TRUE;
}

// Additional warmup done just when CSRSS is being disconnected.
bool CsrssDisconnectWarmup() {
  return ::EnumSystemLocalesEx(EnumLocalesProcEx, LOCALE_WINDOWS, 0, 0);
}

// Cleans up this process if CSRSS will be disconnected, as this disconnection
// is not supported Windows behavior.
// Currently, this step requires closing a heap that this shared with csrss.exe.
// Closing the ALPC Port handle to csrss.exe leaves this heap in an invalid
// state. This causes problems if anyone enumerates the heap.
bool CsrssDisconnectCleanup() {
  HANDLE csr_port_heap = FindCsrPortHeap();
  if (!csr_port_heap) {
    DLOG(ERROR) << "Failed to find CSR Port heap handle";
    return false;
  }
  ::HeapDestroy(csr_port_heap);
  return true;
}

}  // namespace

// Memory buffer mapped from the parent, with our configuration.
SANDBOX_INTERCEPT HandleCloserConfig g_handle_closer_info{};

bool HandleCloserAgent::NeedsHandlesClosed() {
  return g_handle_closer_info.handle_closer_enabled;
}

HandleCloserAgent::HandleCloserAgent()
    : config_(g_handle_closer_info),
      is_csrss_connected_(true),
      dummy_handle_(::CreateEvent(nullptr, false, false, nullptr)) {}

HandleCloserAgent::~HandleCloserAgent() {}

// Attempts to stuff |closed_handle| with a duplicated handle for a dummy Event
// with no access. This should allow the handle to be closed, to avoid
// generating EXCEPTION_INVALID_HANDLE on shutdown, but nothing else. For now
// the only supported type is Event or File.
bool HandleCloserAgent::AttemptToStuffHandleSlot(HANDLE closed_handle) {
  if (!dummy_handle_.is_valid()) {
    return false;
  }

  // This should never happen, as dummy_handle_ is created before closing
  // to_stuff.
  DCHECK(dummy_handle_.get() != closed_handle);

  std::vector<HANDLE> to_close;

  const DWORD original_proc_num = ::GetCurrentProcessorNumber();
  DWORD proc_num = original_proc_num;
  DWORD_PTR original_affinity_mask =
      ::SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << proc_num);
  bool found_handle = false;
  BOOL result = FALSE;

  // There is per-processor based free list of handles entries. The free handle
  // from current processor's freelist is preferred for reusing, so cycling
  // through all possible processors to find closed_handle.
  // Start searching from current processor which covers usual cases.

  do {
    DWORD_PTR current_mask = DWORD_PTR{1} << proc_num;

    if (original_affinity_mask & current_mask) {
      if (proc_num != original_proc_num) {
        ::SetThreadAffinityMask(::GetCurrentThread(), current_mask);
      }

      HANDLE dup_dummy = nullptr;
      size_t count = 16;

      do {
        result =
            ::DuplicateHandle(::GetCurrentProcess(), dummy_handle_.get(),
                              ::GetCurrentProcess(), &dup_dummy, 0, false, 0);
        if (!result) {
          break;
        }
        if (dup_dummy != closed_handle) {
          to_close.push_back(dup_dummy);
        } else {
          found_handle = true;
        }
      } while (count-- && reinterpret_cast<uintptr_t>(dup_dummy) <
                              reinterpret_cast<uintptr_t>(closed_handle));
    }

    proc_num++;
    if (proc_num == sizeof(DWORD_PTR) * 8) {
      proc_num = 0;
    }
    if (proc_num == original_proc_num) {
      break;
    }
  } while (result && !found_handle);

  SetThreadAffinityMask(::GetCurrentThread(), original_affinity_mask);

  for (HANDLE h : to_close) {
    ::CloseHandle(h);
  }

  return found_handle;
}

bool HandleCloserAgent::CloseHandles() {
  CHECK(config_.handle_closer_enabled);

  // Skip closing these handles when Application Verifier is in use in order to
  // avoid invalid-handle exceptions.
  if (base::win::IsAppVerifierLoaded()) {
    return true;
  }

  DWORD handle_count;
  if (!::GetProcessHandleCount(::GetCurrentProcess(), &handle_count)) {
    return false;
  }

  // The system call will return only handles up to the buffer size so add a
  // margin of error of an additional 1000 handles.
  std::vector<char> buffer((handle_count + 1000) * sizeof(uint32_t));
  DWORD return_length;
  NTSTATUS status = GetNtExports()->QueryInformationProcess(
      ::GetCurrentProcess(), static_cast<PROCESSINFOCLASS>(ProcessHandleTable),
      buffer.data(), static_cast<ULONG>(buffer.size()), &return_length);

  if (!NT_SUCCESS(status)) {
    ::SetLastError(GetLastErrorFromNtStatus(status));
    return false;
  }
  DCHECK(buffer.size() >= return_length);
  DCHECK((buffer.size() % sizeof(uint32_t)) == 0);

  base::span<uint32_t> handle_values(reinterpret_cast<uint32_t*>(buffer.data()),
                                     return_length / sizeof(uint32_t));
  for (uint32_t handle_value : handle_values) {
    HANDLE handle = base::win::Uint32ToHandle(handle_value);
    auto type_name = GetTypeNameFromHandle(handle);
    if (type_name) {
      MaybeCloseHandle(type_name.value(), handle);
    }
  }

  return true;
}

bool HandleCloserAgent::MaybeCloseHandle(std::wstring& type_name,
                                         HANDLE handle) {
  bool bClose = false;
  // Determine if the handle should be closed. Avoid matching the type unless
  // we are configured to close it.
  if (config_.section_windows_global_shell_counters && type_name == kSection) {
    auto path = GetPathFromHandle(handle);
    if (path && base::EndsWith(path.value(), kWindowsShellGlobalCounters)) {
      bClose = true;
    }
  } else if ((config_.file_device_api || config_.file_ksecdd) &&
             type_name == kFile) {
    auto path = GetPathFromHandle(handle);
    if (path && config_.file_device_api && path.value() == kDeviceApi) {
      bClose = true;
    }
    if (path && config_.file_ksecdd && path.value() == kDeviceKsecDD) {
      bClose = true;
    }
  } else if (config_.disconnect_csrss && type_name == kALPCPort) {
    // Do csrss extra warmup & heap closing but only once. Normally there is
    // only one ALPC port to close.
    if (is_csrss_connected_) {
      if (!CsrssDisconnectWarmup() || !CsrssDisconnectCleanup()) {
        return false;
      }
      is_csrss_connected_ = false;
    }
    bClose = true;
  }
  // Implement the close.
  if (bClose) {
    // If we can't unprotect or close the handle we should keep going.
    if (!::SetHandleInformation(handle, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0)) {
      return false;
    }
    if (!::CloseHandle(handle)) {
      return false;
    }
    // Attempt to stuff this handle with a new dummy Event.
    if (type_name == kFile) {
      // Note: could also stuff "Event" but do not support closing any Events
      // at the moment.
      return AttemptToStuffHandleSlot(handle);
    }
  }
  return true;
}

}  // namespace sandbox
