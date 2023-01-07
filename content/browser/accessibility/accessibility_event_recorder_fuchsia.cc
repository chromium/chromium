// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder_fuchsia.h"

#include <ostream>

namespace content {

// static
AccessibilityEventRecorderFuchsia*
    AccessibilityEventRecorderFuchsia::instance_ = nullptr;

AccessibilityEventRecorderFuchsia::AccessibilityEventRecorderFuchsia(
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";
  instance_ = this;
}

AccessibilityEventRecorderFuchsia::~AccessibilityEventRecorderFuchsia() {
  instance_ = nullptr;
}

}  // namespace content
