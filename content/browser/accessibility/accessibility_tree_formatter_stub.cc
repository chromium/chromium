// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "ui/base/buildflags.h"

namespace content {

#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)
// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateBlinkFormatter();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AXInspectFactory::CreateBlinkFormatter},
  };
}
#endif

}  // namespace content
