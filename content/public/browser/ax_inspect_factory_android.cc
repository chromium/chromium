// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_android.h"
#include "content/browser/accessibility/accessibility_tree_formatter_android_external.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  // The default platform tree formatter for Android uses the "external" tree,
  // i.e. pulling from the AccessibilityNodeInfo objects in the Java-side code.
  // If the internal tree is desired, then CreateFormatter() should be called
  // with the appropriate tree type.
  return AXInspectFactory::CreateFormatter(ui::AXApiType::kAndroidExternal);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(ui::AXApiType::kAndroid, manager, pid,
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
    case ui::AXApiType::kAndroid:
      return std::make_unique<AccessibilityTreeFormatterAndroid>();
    case ui::AXApiType::kAndroidExternal:
      return std::make_unique<AccessibilityTreeFormatterAndroidExternal>();
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
  return {ui::AXApiType::kBlink, ui::AXApiType::kAndroid};
}

}  // namespace content
