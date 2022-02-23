// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace content {

class BrowserAccessibilityManager;

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
    : public ui::AXEventRecorder {
 public:
  explicit AccessibilityEventRecorderAuraLinux(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const ui::AXTreeSelector& selector);

  AccessibilityEventRecorderAuraLinux(
      const AccessibilityEventRecorderAuraLinux&) = delete;
  AccessibilityEventRecorderAuraLinux& operator=(
      const AccessibilityEventRecorderAuraLinux&) = delete;

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
  // TODO: should be either removed or converted to a weakptr.
  const raw_ptr<BrowserAccessibilityManager> manager_;
  base::ProcessId pid_;
  ui::AXTreeSelector selector_;
  static AccessibilityEventRecorderAuraLinux* instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_AURALINUX_H_
