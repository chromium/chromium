// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_

#include <vector>

#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {
class BrowserAccessibilityAuraLinux;

// Manages a tree of BrowserAccessibilityAuraLinux objects.
class CONTENT_EXPORT BrowserAccessibilityManagerAuraLinux
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAuraLinux(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerAuraLinux() override;

  static ui::AXTreeUpdate GetEmptyDocument();
  static BrowserAccessibility* FindCommonAncestor(
      BrowserAccessibility* object1,
      BrowserAccessibility* object2);

  // Implementation of BrowserAccessibilityManager methods.
  void FireFocusEvent(BrowserAccessibility* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          BrowserAccessibility* node) override;

  void FireSelectedEvent(BrowserAccessibility* node);
  void FireExpandedEvent(BrowserAccessibility* node, bool is_expanded);
  void FireLoadingEvent(BrowserAccessibility* node, bool is_loading);
  void FireNameChangedEvent(BrowserAccessibility* node);
  void FireDescriptionChangedEvent(BrowserAccessibility* node);
  void FireSubtreeCreatedEvent(BrowserAccessibility* node);
  void OnFindInPageResult(int request_id,
                          int match_index,
                          int start_id,
                          int start_offset,
                          int end_id,
                          int end_offset) override;
  void OnFindInPageTermination() override;

 protected:
  // AXTreeObserver methods.
  void OnNodeDataWillChange(ui::AXTree* tree,
                            const ui::AXNodeData& old_node_data,
                            const ui::AXNodeData& new_node_data) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

 private:
  void FireEvent(BrowserAccessibility* node, ax::mojom::Event event);

  AtkObject* parent_object_;

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerAuraLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
