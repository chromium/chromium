// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_

#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <map>
#include <string>
#include <vector>

#include "base/win/scoped_variant.h"

namespace content {

class AccessibilityTreeFormatterUia : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterUia();
  ~AccessibilityTreeFormatterUia() override;

  // AccessibilityTreeFormatterBase:
  base::Value BuildTree(ui::AXPlatformNodeDelegate* start) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;
  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  static const long properties_[];
  static const long patterns_[];
  static const long pattern_properties_[];
  void RecursiveBuildTree(IUIAutomationElement* node,
                          int root_x,
                          int root_y,
                          base::DictionaryValue* dict) const;
  void BuildCacheRequests();
  void BuildCustomPropertiesMap();
  void AddProperties(IUIAutomationElement* node,
                     int root_x,
                     int root_y,
                     base::DictionaryValue* dict) const;
  void AddAnnotationProperties(IUIAutomationElement* node,
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
  void AddCustomProperties(IUIAutomationElement* node,
                           base::DictionaryValue* dict) const;
  std::string GetPropertyName(long property_id) const;
  void WriteProperty(long propertyId,
                     const base::win::ScopedVariant& var,
                     base::DictionaryValue* dict,
                     int root_x = 0,
                     int root_y = 0) const;
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
  std::u16string GetNodeName(IUIAutomationElement* node) const;
  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;
  void ProcessPropertyForOutput(const std::string& property_name,
                                const base::DictionaryValue& dict,
                                std::string& line) const;
  void ProcessValueForOutput(const std::string& name,
                             const base::Value* value,
                             std::string& line) const;
  std::map<long, std::string>& GetCustomPropertiesMap() const;

  Microsoft::WRL::ComPtr<IUIAutomation> uia_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> element_cache_request_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> children_cache_request_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UIA_WIN_H_
