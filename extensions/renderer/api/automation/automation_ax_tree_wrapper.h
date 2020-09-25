// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_

#include "extensions/common/api/automation.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"

struct ExtensionMsg_AccessibilityEventBundleParams;

namespace extensions {

class AutomationInternalCustomBindings;

// A class that wraps one AXTree and all of the additional state
// and helper methods needed to use it for the automation API.
class AutomationAXTreeWrapper : public ui::AXTreeObserver,
                                public ui::AXTreeManager {
 public:
  AutomationAXTreeWrapper(ui::AXTreeID tree_id,
                          AutomationInternalCustomBindings* owner);
  ~AutomationAXTreeWrapper() override;

  // Returns the AutomationAXTreeWrapper that lists |tree_id| as one of its
  // child trees, if any.
  static AutomationAXTreeWrapper* GetParentOfTreeId(ui::AXTreeID tree_id);

  static std::map<ui::AXTreeID, AutomationAXTreeWrapper*>&
  GetChildTreeIDReverseMap();

  ui::AXTree* tree() { return &tree_; }
  AutomationInternalCustomBindings* owner() { return owner_; }

  // Called by AutomationInternalCustomBindings::OnAccessibilityEvents on
  // the AutomationAXTreeWrapper instance for the correct tree corresponding
  // to this event. Unserializes the tree update and calls back to
  // AutomationInternalCustomBindings to fire any automation events needed.
  bool OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);

  // Returns true if this is the desktop tree.
  bool IsDesktopTree() const;

  // Returns whether |node_id| is the focused node in this tree. Accounts for
  // cases where this tree itself is not focused. Behaves similarly to
  // document.activeElement (within the DOM).
  bool IsInFocusChain(int32_t node_id);

  ui::AXTree::Selection GetUnignoredSelection();

  // Returns an AXNode from the underlying tree if it both exists and is not
  // ignored.
  ui::AXNode* GetUnignoredNodeFromId(int32_t id);

  // Accessibility focus is globally set via automation API from js.
  void SetAccessibilityFocus(int32_t node_id);
  ui::AXNode* GetAccessibilityFocusedNode();

  int32_t accessibility_focused_id() { return accessibility_focused_id_; }

  // Updates or gets this wrapper with the latest state of listeners in js.
  void EventListenerAdded(api::automation::EventType event_type,
                          ui::AXNode* node);
  void EventListenerRemoved(api::automation::EventType event_type,
                            ui::AXNode* node);
  bool HasEventListener(api::automation::EventType event_type,
                        ui::AXNode* node);

  // AXTreeManager overrides.
  ui::AXNode* GetNodeFromTree(const ui::AXTreeID tree_id,
                              const ui::AXNode::AXID node_id) const override;
  ui::AXNode* GetNodeFromTree(const ui::AXNode::AXID node_id) const override;
  ui::AXTreeID GetTreeID() const override;
  ui::AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetRootAsAXNode() const override;
  ui::AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

 private:
  // AXTreeObserver overrides.
  void OnNodeDataChanged(ui::AXTree* tree,
                         const ui::AXNodeData& old_node_data,
                         const ui::AXNodeData& new_node_data) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

  ui::AXTreeID tree_id_;
  ui::AXTree tree_;
  AutomationInternalCustomBindings* owner_;
  std::vector<int> deleted_node_ids_;
  std::vector<int> text_changed_node_ids_;
  ui::AXEventGenerator event_generator_;

  int32_t accessibility_focused_id_ = ui::AXNode::kInvalidAXID;

  // Tracks whether a tree change event was sent during unserialization. Tree
  // changes outside of unserialization do not get reflected here. The value is
  // reset after unserialization.
  bool did_send_tree_change_during_unserialization_ = false;

  // Maps a node to a set containing events for which the node has listeners.
  std::map<int32_t, std::set<api::automation::EventType>> node_id_to_events_;

  DISALLOW_COPY_AND_ASSIGN(AutomationAXTreeWrapper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
