// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_test_strings.h"

namespace {

// This class is used instead of base::WaitableEvent to avoid a dependency on
// //base.
class NamedEvent {
 public:
  // Constructs a NamedEvent from the existing event named |name|.
  NamedEvent(const wchar_t* name)
      : handle_(::OpenEvent(EVENT_ALL_ACCESS, /*bInheritHandle=*/FALSE, name)) {
  }

  NamedEvent(const NamedEvent&) = delete;
  NamedEvent& operator=(const NamedEvent&) = delete;

  ~NamedEvent() {
    if (IsValid())
      ::CloseHandle(handle_);
  }

  bool IsValid() const { return handle_ && handle_ != INVALID_HANDLE_VALUE; }

  HANDLE handle() const { return handle_; }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

}  // namespace

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  NamedEvent wait_for_lock_event(
      tracing::loader_lock_sampler_test::kWaitForLockEventName);
  NamedEvent drop_lock_event(
      tracing::loader_lock_sampler_test::kDropLockEventName);
  if (!wait_for_lock_event.IsValid() || !drop_lock_event.IsValid())
    return FALSE;

  // Inform the main thread that the lock is taken.
  if (!SetEvent(wait_for_lock_event.handle()))
    return FALSE;

  // Wait until the main thread tells us to drop the lock and exit.
  WaitForSingleObject(drop_lock_event.handle(), INFINITE);
  return TRUE;
}
