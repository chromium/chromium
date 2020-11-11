// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "base/win/scoped_variant.h"

namespace content {

class AccessibilityTreeFormatterUia : public AccessibilityTreeFormatterBase {
 public:
  AccessibilityTreeFormatterUia();
  ~AccessibilityTreeFormatterUia() override;

  static std::unique_ptr<ui::AXTreeFormatter> CreateUia();

  static void SetUpCommandLineForTestPass(base::CommandLine* command_line);

  // AccessibilityTreeFormatterBase:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* start) override;
  base::Value BuildTreeForWindow(gfx::AcceleratedWidget hwnd) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

 private:
  static const long properties_[];
  static const long patterns_[];
  static const long pattern_properties_[];
  void RecursiveBuildAccessibilityTree(IUIAutomationElement* node,
                                       int root_x,
                                       int root_y,
                                       base::DictionaryValue* dict) const;
  void BuildCacheRequests();
  void AddProperties(IUIAutomationElement* node,
                     int root_x,
                     int root_y,
                     base::DictionaryValue* dict) const;
  void AddExpandCollapseProperties(IUIAutomationElement* node,
                                   base::DictionaryValue* dict) const;
  void AddGridProperties(IUIAutomationElement* node,
                         base::DictionaryValue* dict) const;
  void AddGridItemProperties(IUIAutomationElement* node,
                             base::DictionaryValue* dict) const;
  void AddRangeValueProperties(IUIAutomationElement* node,
                               base::DictionaryValue* dict) const;
  void AddScrollProperties(IUIAutomationElement* node,
                           base::DictionaryValue* dict) const;
  void AddSelectionProperties(IUIAutomationElement* node,
                              base::DictionaryValue* dict) const;
  void AddSelectionItemProperties(IUIAutomationElement* node,
                                  base::DictionaryValue* dict) const;
  void AddTableProperties(IUIAutomationElement* node,
                          base::DictionaryValue* dict) const;
  void AddToggleProperties(IUIAutomationElement* node,
                           base::DictionaryValue* dict) const;
  void AddValueProperties(IUIAutomationElement* node,
                          base::DictionaryValue* dict) const;
  void AddWindowProperties(IUIAutomationElement* node,
                           base::DictionaryValue* dict) const;
  void WriteProperty(long propertyId,
                     const base::win::ScopedVariant& var,
                     int root_x,
                     int root_y,
                     base::DictionaryValue* dict) const;
  // UIA enums have type I4, print formatted string for these when possible
  void WriteI4Property(long propertyId,
                       long lval,
                       base::DictionaryValue* dict) const;
  void WriteUnknownProperty(long propertyId,
                            IUnknown* unk,
                            base::DictionaryValue* dict) const;
  void WriteRectangleProperty(long propertyId,
                              const VARIANT& value,
                              int root_x,
                              int root_y,
                              base::DictionaryValue* dict) const;
  void WriteElementArray(long propertyId,
                         IUIAutomationElementArray* array,
                         base::DictionaryValue* dict) const;
  base::string16 GetNodeName(IUIAutomationElement* node) const;
  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_result = nullptr) override;
  void ProcessPropertyForOutput(const std::string& property_name,
                                const base::DictionaryValue& dict,
                                std::string& line,
                                base::DictionaryValue* filtered_result);
  void ProcessValueForOutput(const std::string& name,
                             const base::Value* value,
                             std::string& line,
                             base::DictionaryValue* filtered_result);
  Microsoft::WRL::ComPtr<IUIAutomation> uia_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> element_cache_request_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> children_cache_request_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
