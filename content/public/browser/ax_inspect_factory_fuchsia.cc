// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "content/browser/accessibility/accessibility_event_recorder_fuchsia.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return CreateFormatter(kFuchsia);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(kFuchsia, manager, pid, selector);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    AXInspectFactory::Type type) {
  switch (type) {
    case kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case kFuchsia:
      return std::make_unique<AccessibilityTreeFormatterFuchsia>();
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreateRecorder(
    AXInspectFactory::Type type,
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  switch (type) {
    case kFuchsia:
      return std::make_unique<AccessibilityEventRecorderFuchsia>(manager, pid,
                                                                 selector);
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

}  // namespace content
