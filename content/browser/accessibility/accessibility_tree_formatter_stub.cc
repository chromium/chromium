// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

namespace content {

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
std::unique_ptr<ui::AXTreeFormatter> AccessibilityTreeFormatter::Create() {
  return AccessibilityTreeFormatterBlink::CreateBlink();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AccessibilityTreeFormatterBlink::CreateBlink},
  };
}
#endif

}  // namespace content
