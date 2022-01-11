// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_WIN_H_

#include <oleacc.h>

#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace content {

class CONTENT_EXPORT AccessibilityEventRecorderWin
    : public ui::AXEventRecorder {
 public:
  // Flag values that specify the way events are handled.
  enum ListenerType {
    kSync,   // Events are handled synchronously.
    kAsync,  // Events are handled asynchronously.
  };

  AccessibilityEventRecorderWin(base::ProcessId pid,
                                const ui::AXTreeSelector& selector,
                                ListenerType listenerType = kSync);

  AccessibilityEventRecorderWin(const AccessibilityEventRecorderWin&) = delete;
  AccessibilityEventRecorderWin& operator=(
      const AccessibilityEventRecorderWin&) = delete;

  ~AccessibilityEventRecorderWin() override;

  // Callback registered by SetWinEventHook. Just calls OnWinEventHook.
  static CALLBACK void WinEventHookThunk(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG obj_id,
                                         LONG child_id,
                                         DWORD event_thread,
                                         DWORD event_time);

 private:
  // Called by the thunk registered by SetWinEventHook. Retrieves accessibility
  // info about the node the event was fired on and appends a string to
  // the event log.
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  HWINEVENTHOOK win_event_hook_handle_;
  static AccessibilityEventRecorderWin* instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_WIN_H_
