// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace content {

using ui::AXTreeSelector;

class BrowserAccessibilityManager;

// Listens for native accessibility events fired by a given
// BrowserAccessibilityManager and saves human-readable log strings for
// each event fired to a vector. Construct an instance of this class to
// begin listening, call GetEventLogs() to get all of the logs so far, and
// destroy it to stop listening.
//
// A log string should be of the form "<event> on <node>" where <event> is
// the name of the event fired (platform-specific) and <node> is information
// about the accessibility node on which the event was fired, for example its
// role and name.
//
// The implementation is highly platform-specific; a subclass is needed for
// each platform does most of the work.
//
// As currently designed, there should only be one instance of this class.
class CONTENT_EXPORT AccessibilityEventRecorder : public ui::AXEventRecorder {
 public:
  // Get a set of factory methods to create event-recorders, one for each test
  // pass; see |DumpAccessibilityTestBase|.
  using EventRecorderFactory = std::unique_ptr<AccessibilityEventRecorder> (*)(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const AXTreeSelector& selector);

  AccessibilityEventRecorder(BrowserAccessibilityManager* manager);

 protected:
  BrowserAccessibilityManager* const manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_H_
