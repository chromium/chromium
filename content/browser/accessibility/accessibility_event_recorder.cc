// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_buildflags.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

AccessibilityEventRecorder::AccessibilityEventRecorder(
    BrowserAccessibilityManager* manager)
    : manager_(manager) {}

AccessibilityEventRecorder::~AccessibilityEventRecorder() = default;

#if !defined(OS_WIN) && !defined(OS_MACOSX) && !BUILDFLAG(USE_ATK)
// static
std::unique_ptr<AccessibilityEventRecorder> AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern) {
  return std::make_unique<AccessibilityEventRecorder>(manager);
}

// static
std::vector<AccessibilityEventRecorder::TestPass>
AccessibilityEventRecorder::GetTestPasses() {
#if defined(OS_ANDROID)
  // Note: Android doesn't do a "blink" pass; the blink tree is different on
  // Android because we exclude inline text boxes, for performance.
  return {{"android", &AccessibilityEventRecorder::Create}};
#else   // defined(OS_ANDROID)
  return {
      {"blink", &AccessibilityEventRecorder::Create},
      {"native", &AccessibilityEventRecorder::Create},
  };
#endif  // defined(OS_ANDROID)
}
#endif

void AccessibilityEventRecorder::OnEvent(const std::string& event) {
  event_logs_.push_back(event);
  if (callback_)
    callback_.Run(event);
}

}  // namespace content
