// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_H_

#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterAndroid
    : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterAndroid();
  ~AccessibilityTreeFormatterAndroid() override;

  base::Value::Dict BuildTree(ui::AXPlatformNodeDelegate* root) const override;
  base::Value::Dict BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value::Dict BuildNode(ui::AXPlatformNodeDelegate* node) const override;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  void RecursiveBuildTree(const ui::AXPlatformNodeDelegate& node,
                          base::Value::Dict* dict) const;

  void AddProperties(const ui::AXPlatformNodeDelegate& node,
                     base::Value::Dict* dict) const;

  std::string ProcessTreeForOutput(
      const base::Value::Dict& node) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_H_
