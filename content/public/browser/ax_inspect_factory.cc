// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

namespace content {

std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateBlinkFormatter() {
  return std::make_unique<AccessibilityTreeFormatterBlink>();
}

}  // namespace content
