// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterBlink
    : public AccessibilityTreeFormatterBase {
 public:
  explicit AccessibilityTreeFormatterBlink();
  ~AccessibilityTreeFormatterBlink() override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* root) override;

  base::Value BuildTreeForWindow(gfx::AcceleratedWidget widget) const override;

  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

  static std::unique_ptr<ui::AXTreeFormatter> CreateBlink();

 private:
  void RecursiveBuildAccessibilityTree(const BrowserAccessibility& node,
                                       base::DictionaryValue* dict) const;

  uint32_t ChildCount(const BrowserAccessibility& node) const;
  BrowserAccessibility* GetChild(const BrowserAccessibility& node,
                                 uint32_t i) const;

  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
