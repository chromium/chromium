// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_WIN_H_

#include <oleacc.h>
#include <wrl/client.h>

#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterWin
    : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterWin();
  ~AccessibilityTreeFormatterWin() override;

  base::Value BuildTree(ui::AXPlatformNodeDelegate* start) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  void RecursiveBuildTree(const Microsoft::WRL::ComPtr<IAccessible> node,
                          base::Value* dict,
                          LONG root_x,
                          LONG root_y) const;

  void AddProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                     base::Value* dict,
                     LONG root_x,
                     LONG root_y) const;
  void AddMSAAProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                         base::Value* dict,
                         LONG root_x,
                         LONG root_y) const;
  void AddSimpleDOMNodeProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                  base::Value* dict) const;
  bool AddIA2Properties(const Microsoft::WRL::ComPtr<IAccessible>,
                        base::Value* dict) const;
  void AddIA2ActionProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                              base::Value* dict) const;
  void AddIA2HypertextProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                 base::Value* dict) const;
  void AddIA2TextProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                            base::Value* dict) const;
  void AddIA2TableProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                             base::Value* dict) const;
  void AddIA2TableCellProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                 base::Value* dict) const;
  void AddIA2ValueProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                             base::Value* dict) const;
  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;

  // Returns a document accessible object for an active tab in a browser.
  Microsoft::WRL::ComPtr<IAccessible> FindActiveDocument(
      IAccessible* root) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_WIN_H_
