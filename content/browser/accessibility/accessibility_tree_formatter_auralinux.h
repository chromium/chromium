// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace ui {
class AXPlatformNodeAuraLinux;
}

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterAuraLinux
    : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterAuraLinux();
  ~AccessibilityTreeFormatterAuraLinux() override;

 private:
  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;

  base::Value BuildTree(ui::AXPlatformNodeDelegate* root) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;

  std::string EvaluateScript(
      const AXTreeSelector& selector,
      const ui::AXInspectScenario& scenario) const override;

  void RecursiveBuildTree(AtspiAccessible* node, base::Value::Dict* dict) const;
  void RecursiveBuildTree(AtkObject*, base::Value::Dict*) const;

  void AddProperties(AtkObject*, base::Value::Dict*) const;
  void AddProperties(AtspiAccessible*, base::Value::Dict*) const;

  void AddTextProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddHypertextProperties(AtkObject* atk_object,
                              base::Value::Dict* dict) const;
  void AddActionProperties(AtkObject* atk_object,
                           base::Value::Dict* dict) const;
  void AddValueProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddTableProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddTableCellProperties(const ui::AXPlatformNodeAuraLinux* node,
                              AtkObject* atk_object,
                              base::Value::Dict* dict) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_AURALINUX_H_
