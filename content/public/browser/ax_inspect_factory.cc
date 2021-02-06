// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/base/buildflags.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateBlinkFormatter() {
  return CreateFormatter(kBlink);
}

#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(kBlink);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(kBlink);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    AXInspectFactory::Type type) {
  switch (type) {
    case kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
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
    const AXTreeSelector& selector) {
  NOTREACHED() << "Unsupported inspect type " << type;
  return nullptr;
}

#endif

AXInspectFactory::Type::operator std::string() const {
  switch (type_) {
    case kAndroid:
      return "android";
    case kBlink:
      return "blink";
    case kMac:
      return "mac";
    case kLinux:
      return "linux";
    case kWinIA2:
      return "win";
    case kWinUIA:
      return "uia";
    default:
      return "unknown";
  }
}

}  // namespace content
