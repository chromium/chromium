// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_closer_agent.h"

#include <stddef.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/win/static_constants.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/win_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {

// Memory buffer mapped from the parent, with the list of handles.
SANDBOX_INTERCEPT HandleCloserInfo* g_handles_to_close = nullptr;

bool HandleCloserAgent::NeedsHandlesClosed() {
  return !!g_handles_to_close;
}

HandleCloserAgent::HandleCloserAgent()
    : dummy_handle_(::CreateEvent(nullptr, false, false, nullptr)) {}

HandleCloserAgent::~HandleCloserAgent() {}

// Attempts to stuff |closed_handle| with a duplicated handle for a dummy Event
// with no access. This should allow the handle to be closed, to avoid
// generating EXCEPTION_INVALID_HANDLE on shutdown, but nothing else. For now
// the only supported |type| is Event or File.
bool HandleCloserAgent::AttemptToStuffHandleSlot(HANDLE closed_handle,
                                                 const std::wstring& type) {
  // Only attempt to stuff Files and Events at the moment.
  if (type != L"Event" && type != L"File") {
    return true;
  }

  if (!dummy_handle_.IsValid())
    return false;

  // This should never happen, as g_dummy is created before closing to_stuff.
  DCHECK(dummy_handle_.Get() != closed_handle);

  std::vector<HANDLE> to_close;

  const DWORD original_proc_num = GetCurrentProcessorNumber();
  DWORD proc_num = original_proc_num;
  DWORD_PTR original_affinity_mask =
      SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << proc_num);
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
        SetThreadAffinityMask(GetCurrentThread(), current_mask);
      }

      HANDLE dup_dummy = nullptr;
      size_t count = 16;

      do {
        result =
            ::DuplicateHandle(::GetCurrentProcess(), dummy_handle_.Get(),
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

  SetThreadAffinityMask(GetCurrentThread(), original_affinity_mask);

  for (HANDLE h : to_close)
    ::CloseHandle(h);

  return found_handle;
}

// Reads g_handles_to_close and creates the lookup map.
void HandleCloserAgent::InitializeHandlesToClose(bool* is_csrss_connected) {
  CHECK(g_handles_to_close);

  // Default to connected state
  *is_csrss_connected = true;

  // Grab the header.
  HandleListEntry* entry = g_handles_to_close->handle_entries;
  for (size_t i = 0; i < g_handles_to_close->num_handle_types; ++i) {
    // Set the type name.
    wchar_t* input = entry->handle_type;
    if (!wcscmp(input, L"ALPC Port")) {
      *is_csrss_connected = false;
    }
    HandleMap::mapped_type& handle_names = handles_to_close_[input];
    input = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(entry) +
                                       entry->offset_to_names);
    // Grab all the handle names.
    for (size_t j = 0; j < entry->name_count; ++j) {
      std::pair<HandleMap::mapped_type::iterator, bool> name =
          handle_names.insert(input);
      CHECK(name.second);
      input += name.first->size() + 1;
    }

    // Move on to the next entry.
    entry = reinterpret_cast<HandleListEntry*>(reinterpret_cast<char*>(entry) +
                                               entry->record_bytes);

    DCHECK(reinterpret_cast<wchar_t*>(entry) >= input);
    DCHECK(reinterpret_cast<wchar_t*>(entry) - input <
           static_cast<ptrdiff_t>(sizeof(size_t) / sizeof(wchar_t)));
  }

  // Clean up the memory we copied over.
  ::VirtualFree(g_handles_to_close, 0, MEM_RELEASE);
  g_handles_to_close = nullptr;
}

bool HandleCloserAgent::CloseHandles() {
  // Skip closing these handles when Application Verifier is in use in order to
  // avoid invalid-handle exceptions.
  if (base::win::IsAppVerifierLoaded())
    return true;
  // If the accurate handle enumeration fails then fallback to the old brute
  // force approach. This should only happen on Windows 7 and 8.0.
  absl::optional<ProcessHandleMap> handle_map = GetCurrentProcessHandles();
  if (!handle_map)
    return false;

  for (const HandleMap::value_type& handle_to_close : handles_to_close_) {
    ProcessHandleMap::iterator result = handle_map->find(handle_to_close.first);
    if (result == handle_map->end())
      continue;
    const HandleMap::mapped_type& names = handle_to_close.second;
    for (HANDLE handle : result->second) {
      // Empty set means close all handles of this type; otherwise check name.
      if (!names.empty()) {
        auto handle_name = GetPathFromHandle(handle);
        // Move on to the next handle if this name doesn't match.
        if (!handle_name || !names.count(handle_name.value())) {
          continue;
        }
      }

      // If we can't unprotect or close the handle we should keep going.
      if (!::SetHandleInformation(handle, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0))
        continue;
      if (!::CloseHandle(handle))
        continue;
      // Attempt to stuff this handle with a new dummy Event.
      AttemptToStuffHandleSlot(handle, result->first);
    }
  }

  return true;
}

}  // namespace sandbox
