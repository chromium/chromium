// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

AccessibilityEventRecorder::AccessibilityEventRecorder(
    BrowserAccessibilityManager* manager)
    : manager_(manager) {}

}  // namespace content
