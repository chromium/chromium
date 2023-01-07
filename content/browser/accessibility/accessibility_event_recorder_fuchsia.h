// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_FUCHSIA_H_

#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace content {

class CONTENT_EXPORT AccessibilityEventRecorderFuchsia
    : public ui::AXEventRecorder {
 public:
  AccessibilityEventRecorderFuchsia(base::ProcessId pid,
                                    const ui::AXTreeSelector& selector);

  AccessibilityEventRecorderFuchsia(const AccessibilityEventRecorderFuchsia&) =
      delete;
  AccessibilityEventRecorderFuchsia& operator=(
      const AccessibilityEventRecorderFuchsia&) = delete;

  ~AccessibilityEventRecorderFuchsia() override;

 private:
  static AccessibilityEventRecorderFuchsia* instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_FUCHSIA_H_
