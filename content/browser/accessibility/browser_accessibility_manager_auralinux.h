// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"

namespace content {

class BrowserAccessibilityAuraLinux;
class WebAXPlatformTreeManagerDelegate;

// Manages a tree of BrowserAccessibilityAuraLinux objects.
class CONTENT_EXPORT BrowserAccessibilityManagerAuraLinux
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAuraLinux(
      const ui::AXTreeUpdate& initial_tree,
      WebAXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerAuraLinux(
      const BrowserAccessibilityManagerAuraLinux&) = delete;
  BrowserAccessibilityManagerAuraLinux& operator=(
      const BrowserAccessibilityManagerAuraLinux&) = delete;

  ~BrowserAccessibilityManagerAuraLinux() override;

  static ui::AXTreeUpdate GetEmptyDocument();
  static BrowserAccessibility* FindCommonAncestor(
      BrowserAccessibility* object1,
      BrowserAccessibility* object2);

  // AXTreeManager overrides.
  void FireFocusEvent(ui::AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          const ui::AXNode* node) override;

  void FireSelectedEvent(BrowserAccessibility* node);
  void FireEnabledChangedEvent(BrowserAccessibility* node);
  void FireExpandedEvent(BrowserAccessibility* node, bool is_expanded);
  void FireShowingEvent(BrowserAccessibility* node, bool is_showing);
  void FireInvalidStatusChangedEvent(BrowserAccessibility* node);
  void FireAriaCurrentChangedEvent(BrowserAccessibility* node);
  void FireBusyChangedEvent(BrowserAccessibility* node, bool is_busy);
  void FireLoadingEvent(BrowserAccessibility* node, bool is_loading);
  void FireNameChangedEvent(BrowserAccessibility* node);
  void FireDescriptionChangedEvent(BrowserAccessibility* node);
  void FireParentChangedEvent(BrowserAccessibility* node);
  void FireReadonlyChangedEvent(BrowserAccessibility* node);
  void FireSortDirectionChangedEvent(BrowserAccessibility* node);
  void FireTextAttributesChangedEvent(BrowserAccessibility* node);
  void FireSubtreeCreatedEvent(BrowserAccessibility* node);
  void OnFindInPageResult(int request_id,
                          int match_index,
                          int start_id,
                          int start_offset,
                          int end_id,
                          int end_offset) override;
  void OnFindInPageTermination() override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(BrowserAccessibilityManagerAuraLinuxTest,
                           TestEmitChildrenChanged);
  // AXTreeObserver methods.
  void OnIgnoredWillChange(
      ui::AXTree* tree,
      ui::AXNode* node,
      bool is_ignored_new_value,
      bool is_changing_unignored_parents_children) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

 private:
  bool CanEmitChildrenChanged(BrowserAccessibility* node) const;
  void FireEvent(BrowserAccessibility* node, ax::mojom::Event event);

  raw_ptr<AtkObject> parent_object_;

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_AURALINUX_H_
