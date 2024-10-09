// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_android.h"
#include "content/browser/accessibility/accessibility_tree_formatter_android_external.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace content {

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformFormatterType() {
  return ui::AXApiType::kAndroidExternal;
}

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformRecorderType() {
  return ui::AXApiType::kAndroid;
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    ui::AXApiType::Type type) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  ui::AXTreeManager::AlwaysFailFast();

  switch (type) {
    case ui::AXApiType::kAndroid:
      return std::make_unique<AccessibilityTreeFormatterAndroid>();
    case ui::AXApiType::kAndroidExternal:
      return std::make_unique<AccessibilityTreeFormatterAndroidExternal>();
    case ui::AXApiType::kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported API type " << static_cast<std::string>(type);
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

  NOTREACHED_IN_MIGRATION()
      << "Unsupported API type " << static_cast<std::string>(type);
  return nullptr;
}

// static
std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink, ui::AXApiType::kAndroid,
          ui::AXApiType::kAndroidExternal};
}

}  // namespace content
