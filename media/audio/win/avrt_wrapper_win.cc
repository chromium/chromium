// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/avrt_wrapper_win.h"

#include <iterator>

#include "base/check.h"
#include "base/win/win_util.h"

namespace avrt {

// Function pointers
typedef BOOL (WINAPI *AvRevertMmThreadCharacteristicsFn)(HANDLE);
typedef HANDLE (WINAPI *AvSetMmThreadCharacteristicsFn)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *AvSetMmThreadPriorityFn)(HANDLE, AVRT_PRIORITY);

HMODULE g_avrt = NULL;
AvRevertMmThreadCharacteristicsFn g_revert_mm_thread_characteristics = NULL;
AvSetMmThreadCharacteristicsFn g_set_mm_thread_characteristics = NULL;
AvSetMmThreadPriorityFn g_set_mm_thread_priority = NULL;

bool Initialize() {
  if (!g_set_mm_thread_priority) {
    // The avrt.dll is available on Windows Vista and later.
    auto path = base::win::ExpandEnvironmentVariables(
        L"%SystemRoot%\\system32\\avrt.dll");
    if (!path) {
      return false;
    }

    g_avrt = LoadLibraryExW(path->c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_avrt) {
      return false;
    }

    g_revert_mm_thread_characteristics =
        reinterpret_cast<AvRevertMmThreadCharacteristicsFn>(
            GetProcAddress(g_avrt, "AvRevertMmThreadCharacteristics"));
    g_set_mm_thread_characteristics =
        reinterpret_cast<AvSetMmThreadCharacteristicsFn>(
            GetProcAddress(g_avrt, "AvSetMmThreadCharacteristicsW"));
    g_set_mm_thread_priority = reinterpret_cast<AvSetMmThreadPriorityFn>(
        GetProcAddress(g_avrt, "AvSetMmThreadPriority"));
  }

  return (g_avrt && g_revert_mm_thread_characteristics &&
          g_set_mm_thread_characteristics && g_set_mm_thread_priority);
}

bool AvRevertMmThreadCharacteristics(HANDLE avrt_handle) {
  DCHECK(g_revert_mm_thread_characteristics);
  return (g_revert_mm_thread_characteristics &&
          g_revert_mm_thread_characteristics(avrt_handle));
}

HANDLE AvSetMmThreadCharacteristics(const wchar_t* task_name,
                                    DWORD* task_index) {
  DCHECK(g_set_mm_thread_characteristics);
  return (g_set_mm_thread_characteristics ?
          g_set_mm_thread_characteristics(task_name, task_index) : NULL);
}

bool AvSetMmThreadPriority(HANDLE avrt_handle, AVRT_PRIORITY priority) {
  DCHECK(g_set_mm_thread_priority);
  return (g_set_mm_thread_priority &&
          g_set_mm_thread_priority(avrt_handle, priority));
}

}  // namespace avrt
