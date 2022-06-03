// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_MAC_H_

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

@class BrowserAccessibilityCocoa;

namespace content {

class CONTENT_EXPORT AccessibilityTreeFormatterMac
    : public ui::AXTreeFormatterBase {
 public:
  AccessibilityTreeFormatterMac();
  ~AccessibilityTreeFormatterMac() override;

  base::Value BuildTree(ui::AXPlatformNodeDelegate* root) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value BuildNode(ui::AXPlatformNodeDelegate* node) const override;

  std::string EvaluateScript(
      ui::AXPlatformNodeDelegate* root,
      const std::vector<ui::AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const override;

 protected:
  void AddDefaultFilters(
      std::vector<ui::AXPropertyFilter>* property_filters) override;

 private:
  base::Value BuildTree(const id root) const;
  base::Value BuildTreeForAXUIElement(AXUIElementRef node) const;

  base::Value BuildNode(const id node) const;

  void RecursiveBuildTree(const id node,
                          const NSRect& root_rect,
                          const a11y::LineIndexer* line_indexer,
                          base::Value* dict) const;

  void AddProperties(const id node,
                     const NSRect& root_rect,
                     const a11y::LineIndexer* line_indexer,
                     base::Value* dict) const;

  // Invokes an attributes by a property node.
  a11y::OptionalNSObject InvokeAttributeFor(
      const BrowserAccessibilityCocoa* cocoa_node,
      const ui::AXPropertyNode& property_node,
      const a11y::LineIndexer* line_indexer) const;

  base::Value PopulateLocalPosition(const id node,
                                    const NSRect& root_rect) const;
  base::Value PopulatePoint(NSPoint) const;
  base::Value PopulateSize(NSSize) const;
  base::Value PopulateRect(NSRect) const;
  base::Value PopulateRange(NSRange) const;
  base::Value PopulateTextPosition(
      const BrowserAccessibility::AXPosition& position,
      const a11y::LineIndexer* line_indexer) const;
  base::Value PopulateTextMarkerRange(
      id marker_range,
      const a11y::LineIndexer* line_indexer) const;
  base::Value PopulateObject(id, const a11y::LineIndexer* line_indexer) const;
  base::Value PopulateArray(NSArray*,
                            const a11y::LineIndexer* line_indexer) const;

  std::string NodeToLineIndex(id, const a11y::LineIndexer*) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_MAC_H_
