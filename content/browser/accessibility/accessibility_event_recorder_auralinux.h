// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/common/content_export.h"

namespace content {

// This class has two distinct event recording code paths. When we are
// recording events in-process (typically this is used for
// DumpAccessibilityEvents tests), we use ATK's global event handlers. Since
// ATK doesn't support intercepting events from other processes, if we have a
// non-zero PID or an accessibility application name pattern, we use AT-SPI2
// directly to intercept events.
// TODO(crbug.com/1133330) AT-SPI2 should be capable of intercepting events
// in-process as well, thus it should be possible to remove the ATK code path
// entirely.
class CONTENT_EXPORT AccessibilityEventRecorderAuraLinux
    : public AccessibilityEventRecorder {
 public:
  explicit AccessibilityEventRecorderAuraLinux(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const AXTreeSelector& selector);
  ~AccessibilityEventRecorderAuraLinux() override;

  void ProcessATKEvent(const char* event,
                       unsigned int n_params,
                       const GValue* params);
  void ProcessATSPIEvent(const AtspiEvent* event);

  static gboolean OnATKEventReceived(GSignalInvocationHint* hint,
                                     unsigned int n_params,
                                     const GValue* params,
                                     gpointer data);

 private:
  bool ShouldUseATSPI();

  std::string AtkObjectToString(AtkObject* obj, bool include_name);
  void AddATKEventListener(const char* event_name);
  void AddATKEventListeners();
  void RemoveATKEventListeners();
  bool IncludeState(AtkStateType state_type);

  void AddATSPIEventListeners();
  void RemoveATSPIEventListeners();

  AtspiEventListener* atspi_event_listener_ = nullptr;
  base::ProcessId pid_;
  ui::AXTreeSelector selector_;
  static AccessibilityEventRecorderAuraLinux* instance_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderAuraLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_
