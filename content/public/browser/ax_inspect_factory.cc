// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/base/buildflags.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateBlinkFormatter() {
  return CreateFormatter(ui::AXApiType::kBlink);
}

#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(ui::AXApiType::kBlink);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(ui::AXApiType::kBlink);
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

  NOTREACHED() << "Unsupported API type " << static_cast<std::string>(type);
  return nullptr;
}

// static
std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink};
}

#endif

}  // namespace content
