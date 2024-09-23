// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/win/com_init_util.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_win.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_win_uia.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_uia_win.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_win.h"

namespace content {

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformFormatterType() {
  return ui::AXApiType::kWinIA2;
}

// static
ui::AXApiType::Type AXInspectFactory::DefaultPlatformRecorderType() {
  return ui::AXApiType::kWinIA2;
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
    case ui::AXApiType::kWinIA2:
      base::win::AssertComInitialized();
      return std::make_unique<ui::AXTreeFormatterWin>();
    case ui::AXApiType::kWinUIA:
      base::win::AssertComInitialized();
      return std::make_unique<ui::AXTreeFormatterUia>();
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported API type " << std::string_view(type);
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

  if (!selector.pattern.empty()) {
    LOG(FATAL) << "Recording accessibility events from an application name "
                  "match pattern not supported on this platform yet.";
  }

  switch (type) {
    case ui::AXApiType::kWinIA2: {
      // For now, just use out of context events when running as a utility to
      // watch events (no BrowserAccessibilityManager), because otherwise Chrome
      // events are not getting reported. Being in context is better so that for
      // TEXT_REMOVED and TEXT_INSERTED events, we can query the text that was
      // inserted or removed and include that in the log.
      return std::make_unique<ui::AXEventRecorderWin>(
          pid, selector,
          manager ? ui::AXEventRecorderWin::kSync
                  : ui::AXEventRecorderWin::kAsync);
    }
    case ui::AXApiType::kWinUIA:
      return std::make_unique<ui::AXEventRecorderWinUia>(selector);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported API type " << std::string_view(type);
  }
  return nullptr;
}

std::vector<ui::AXApiType::Type> AXInspectFactory::SupportedApis() {
  return {ui::AXApiType::kBlink, ui::AXApiType::kWinIA2,
          ui::AXApiType::kWinUIA};
}

}  // namespace content
