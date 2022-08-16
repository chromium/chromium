// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_FUCHSIA_H_

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace content {

// Class for generalizing human-readable AXTree dumps.
class CONTENT_EXPORT AccessibilityTreeFormatterFuchsia
    : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterFuchsia();
  ~AccessibilityTreeFormatterFuchsia() override;

  AccessibilityTreeFormatterFuchsia(const AccessibilityTreeFormatterFuchsia&) =
      delete;
  AccessibilityTreeFormatterFuchsia& operator=(
      const AccessibilityTreeFormatterFuchsia&) = delete;

  // ui::AXTreeFormatterBase overrides.
  base::Value BuildTree(ui::AXPlatformNodeDelegate* root) const override;
  base::Value BuildTreeForSelector(const AXTreeSelector&) const override;
  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  void RecursiveBuildTree(const BrowserAccessibility& node,
                          base::Value::Dict* dict) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;

  void AddProperties(const BrowserAccessibility& node,
                     base::Value::Dict* dict) const;
};

}  // namespace content

#endif  // SRC_CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_FUCHSIA_H_
