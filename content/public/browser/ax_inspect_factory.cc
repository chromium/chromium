// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/base/buildflags.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateBlinkFormatter() {
  return CreateFormatter(ui::AXApiType::kBlink);
}

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(DefaultPlatformFormatterType());
}

// TODO(crbug.com/336611337): Add iOS-specific AXInspectorFactory logic.
#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT) || BUILDFLAG(IS_IOS)

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformFormatterType() {
  return ui::AXApiType::kBlink;
}

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformRecorderType() {
  return ui::AXApiType::kBlink;
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

  DUMP_WILL_BE_NOTREACHED()
      << "Unsupported API type " << static_cast<std::string>(type);
  return nullptr;
}

// static
std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink};
}

#endif

}  // namespace content
