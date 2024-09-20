// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_mac.h"

namespace content {

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformFormatterType() {
  return ui::AXApiType::kMac;
}

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformRecorderType() {
  return ui::AXApiType::kMac;
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    ui::AXApiType::Type type) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  ui::AXTreeManager::AlwaysFailFast();

  switch (type) {
    case ui::AXApiType::kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case ui::AXApiType::kMac:
      return std::make_unique<ui::AXTreeFormatterMac>();
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported API type " << type;
  }
  return nullptr;
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreateRecorder(
    ui::AXApiType::Type type,
    ui::AXPlatformTreeManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  ui::AXTreeManager::AlwaysFailFast();

  switch (type) {
    case ui::AXApiType::kMac:
      return std::make_unique<ui::AXEventRecorderMac>(manager->GetWeakPtr(),
                                                      pid, selector);
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported API type " << type;
  }
  return nullptr;
}

// static
std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink, ui::AXApiType::kMac};
}

}  // namespace content
