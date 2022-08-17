// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_

#include "extensions/common/api/automation.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"

struct ExtensionMsg_AccessibilityEventBundleParams;

namespace extensions {

class AutomationInternalCustomBindings;

// A class that wraps one AXTree and all of the additional state
// and helper methods needed to use it for the automation API.
class AutomationAXTreeWrapper : public ui::AXTreeManager {
 public:
  AutomationAXTreeWrapper(ui::AXTreeID tree_id,
                          AutomationInternalCustomBindings* owner);

  AutomationAXTreeWrapper(const AutomationAXTreeWrapper&) = delete;
  AutomationAXTreeWrapper& operator=(const AutomationAXTreeWrapper&) = delete;

  ~AutomationAXTreeWrapper() override;

  // Returns the AutomationAXTreeWrapper that lists |tree_id| as one of its
  // child trees, if any.
  static AutomationAXTreeWrapper* GetParentOfTreeId(ui::AXTreeID tree_id);

  static std::map<ui::AXTreeID, AutomationAXTreeWrapper*>&
  GetChildTreeIDReverseMap();

  // Multiroot tree lookup.
  static ui::AXNode* GetParentTreeNodeForAppID(
      const std::string& app_id,
      const AutomationInternalCustomBindings* owner);
  static AutomationAXTreeWrapper* GetParentTreeWrapperForAppID(
      const std::string& app_id,
      const AutomationInternalCustomBindings* owner);
  static std::vector<ui::AXNode*> GetChildTreeNodesForAppID(
      const std::string& app_id,
      const AutomationInternalCustomBindings* owner);

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

  // Returns whether this tree is scaled by a device scale factor.
  bool HasDeviceScaleFactor() const;

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

  // Gets the parent tree wrapper.
  AutomationAXTreeWrapper* GetParentTree();

  // Gets the first tree wrapper with an unignored root. This can be |this| tree
  // wrapper or an ancestor. A root is ignored if the tree has valid nodes with
  // a valid ax::mojom::StringAttribute::kChildTreeNodeAppId making the tree
  // have multiple roots.
  AutomationAXTreeWrapper* GetTreeWrapperWithUnignoredRoot();

  // Gets a parent tree wrapper by following the first valid parent tree node
  // app id. Useful if this tree contains app ids that always reference the same
  // parent tree.
  AutomationAXTreeWrapper* GetParentTreeFromAnyAppID();

  // Updates or gets this wrapper with the latest state of listeners in js.
  void EventListenerAdded(api::automation::EventType event_type,
                          ui::AXNode* node);
  void EventListenerRemoved(api::automation::EventType event_type,
                            ui::AXNode* node);
  bool HasEventListener(api::automation::EventType event_type,
                        ui::AXNode* node);
  size_t EventListenerCount() const;

  // Indicates whether this tree is ignored due to a hosting ancestor tree/node
  // being ignored.
  bool IsTreeIgnored();

  // AXTreeManager overrides.
  ui::AXNode* GetNodeFromTree(const ui::AXTreeID tree_id,
                              const ui::AXNodeID node_id) const override;
  ui::AXNode* GetNodeFromTree(const ui::AXNodeID node_id) const override;
  ui::AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

 private:
  // AXTreeObserver overrides.
  void OnNodeDataChanged(ui::AXTree* tree,
                         const ui::AXNodeData& old_node_data,
                         const ui::AXNodeData& new_node_data) override;
  void OnStringAttributeChanged(ui::AXTree* tree,
                                ui::AXNode* node,
                                ax::mojom::StringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
  void OnIgnoredChanged(ui::AXTree* tree,
                        ui::AXNode* node,
                        bool is_ignored_new_value) override;

  AutomationInternalCustomBindings* owner_;
  std::vector<int> deleted_node_ids_;
  std::vector<int> text_changed_node_ids_;

  int32_t accessibility_focused_id_ = ui::kInvalidAXNodeID;

  // Tracks whether a tree change event was sent during unserialization. Tree
  // changes outside of unserialization do not get reflected here. The value is
  // reset after unserialization.
  bool did_send_tree_change_during_unserialization_ = false;

  // Maps a node to a set containing events for which the node has listeners.
  std::map<int32_t, std::set<api::automation::EventType>> node_id_to_events_;

  // A collection of all nodes with
  // ax::mojom::StringAttribute::kAppId set.
  std::set<std::string> all_tree_node_app_ids_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
