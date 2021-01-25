// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "content/browser/accessibility/accessibility_tree_formatter_auralinux.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(kLinux);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    AXInspectFactory::Type type) {
  switch (type) {
    case kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case kLinux:
      return std::make_unique<AccessibilityTreeFormatterAuraLinux>();
    default:
      NOTREACHED() << "Unsupported formatter type " << type;
  }
  return nullptr;
}

}  // namespace content
