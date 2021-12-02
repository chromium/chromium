// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/win/com_init_util.h"
#include "content/browser/accessibility/accessibility_event_recorder_uia_win.h"
#include "content/browser/accessibility/accessibility_event_recorder_win.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_uia_win.h"
#include "content/browser/accessibility/accessibility_tree_formatter_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return CreateFormatter(ui::AXApiType::kWinIA2);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(ui::AXApiType::kWinIA2, manager, pid,
                                          selector);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    ui::AXApiType::Type type) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  BrowserAccessibilityManager::AlwaysFailFast();

  switch (type) {
    case ui::AXApiType::kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case ui::AXApiType::kWinIA2:
      base::win::AssertComInitialized();
      return std::make_unique<AccessibilityTreeFormatterWin>();
    case ui::AXApiType::kWinUIA:
      base::win::AssertComInitialized();
      return std::make_unique<AccessibilityTreeFormatterUia>();
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreateRecorder(
    ui::AXApiType::Type type,
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  BrowserAccessibilityManager::AlwaysFailFast();

  if (!selector.pattern.empty()) {
    LOG(FATAL) << "Recording accessibility events from an application name "
                  "match pattern not supported on this platform yet.";
  }

  switch (type) {
    case ui::AXApiType::kWinIA2:
      return std::make_unique<AccessibilityEventRecorderWin>(manager, pid,
                                                             selector);
    case ui::AXApiType::kWinUIA:
      return std::make_unique<AccessibilityEventRecorderUia>(manager, pid,
                                                             selector.pattern);
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

}  // namespace content
