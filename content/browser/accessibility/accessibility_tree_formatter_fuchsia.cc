// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"

namespace content {

AccessibilityTreeFormatterFuchsia::AccessibilityTreeFormatterFuchsia() =
    default;
AccessibilityTreeFormatterFuchsia::~AccessibilityTreeFormatterFuchsia() =
    default;

base::Value AccessibilityTreeFormatterFuchsia::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  return base::Value();
}

std::string AccessibilityTreeFormatterFuchsia::ProcessTreeForOutput(
    const base::DictionaryValue& node) const {
  return std::string();
}

base::Value AccessibilityTreeFormatterFuchsia::BuildTreeForSelector(
    const AXTreeSelector&) const {
  return base::Value();
}

}  // namespace content
