// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_auralinux.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(ui::AXApiType::kLinux);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(ui::AXApiType::kLinux, manager, pid,
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
    case ui::AXApiType::kLinux:
      return std::make_unique<ui::AXTreeFormatterAuraLinux>();
    default:
      NOTREACHED() << "Unsupported API type " << static_cast<std::string>(type);
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

  switch (type) {
    case ui::AXApiType::kLinux:
      return std::make_unique<ui::AXEventRecorderAuraLinux>(manager, pid,
                                                            selector);
    default:
      NOTREACHED() << "Unsupported API type " << static_cast<std::string>(type);
  }
  return nullptr;
}

// static
std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink, ui::AXApiType::kLinux};
}

}  // namespace content
