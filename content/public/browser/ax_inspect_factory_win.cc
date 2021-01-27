// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "base/notreached.h"
#include "base/win/com_init_util.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_uia_win.h"
#include "content/browser/accessibility/accessibility_tree_formatter_win.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return CreateFormatter(kWinIA2);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    AXInspectFactory::Type type) {
  switch (type) {
    case kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case kWinIA2:
      base::win::AssertComInitialized();
      return std::make_unique<AccessibilityTreeFormatterWin>();
    case kWinUIA:
      base::win::AssertComInitialized();
      return std::make_unique<AccessibilityTreeFormatterUia>();
    default:
      NOTREACHED() << "Unsupported formatter type " << type;
  }
  return nullptr;
}

}  // namespace content
