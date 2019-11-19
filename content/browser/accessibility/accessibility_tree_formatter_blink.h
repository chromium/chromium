// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/browser/accessibility/accessibility_tree_formatter_browser.h"

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterBlink
    : public AccessibilityTreeFormatterBrowser {
 public:
  explicit AccessibilityTreeFormatterBlink();
  ~AccessibilityTreeFormatterBlink() override;

  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;
  static std::unique_ptr<AccessibilityTreeFormatter> CreateBlink();

 private:
  base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  const std::string GetDenyNodeString() override;
  uint32_t ChildCount(const BrowserAccessibility& node) const override;
  BrowserAccessibility* GetChild(const BrowserAccessibility& node,
                                 uint32_t i) const override;
  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict) override;
  base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_BLINK_H_
