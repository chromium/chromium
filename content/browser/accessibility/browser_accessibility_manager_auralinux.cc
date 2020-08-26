// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_auralinux.h"

#include <atk/atk.h>

#include <set>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerAuraLinux(initial_tree, delegate);
}

BrowserAccessibilityManagerAuraLinux*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAuraLinux() {
  return static_cast<BrowserAccessibilityManagerAuraLinux*>(this);
}

BrowserAccessibilityManagerAuraLinux::BrowserAccessibilityManagerAuraLinux(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate)
    : BrowserAccessibilityManager(delegate) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerAuraLinux::~BrowserAccessibilityManagerAuraLinux() {}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerAuraLinux::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 1;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAuraLinux::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  FireEvent(node, ax::mojom::Event::kFocus);
}

void BrowserAccessibilityManagerAuraLinux::FireSelectedEvent(
    BrowserAccessibility* node) {
  // Some browser UI widgets, such as the omnibox popup, only send notifications
  // when they become selected. In contrast elements in a page, such as options
  // in the select element, also send notifications when they become unselected.
  // Since AXPlatformNodeAuraLinux must handle firing a platform event for the
  // unselected case, we can safely ignore the unselected case for rendered
  // elements.
  if (!node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    return;

  FireEvent(node, ax::mojom::Event::kSelection);
}

void BrowserAccessibilityManagerAuraLinux::FireLoadingEvent(
    BrowserAccessibility* node,
    bool is_loading) {
  gfx::NativeViewAccessible obj = node->GetNativeViewAccessible();
  if (!ATK_IS_OBJECT(obj))
    return;

  atk_object_notify_state_change(obj, ATK_STATE_BUSY, is_loading);
  if (!is_loading)
    g_signal_emit_by_name(obj, "load_complete");
}

void BrowserAccessibilityManagerAuraLinux::FireEnabledChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnEnabledChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireExpandedEvent(
    BrowserAccessibility* node,
    bool is_expanded) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnExpandedStateChanged(
      is_expanded);
}

void BrowserAccessibilityManagerAuraLinux::FireEvent(BrowserAccessibility* node,
                                                     ax::mojom::Event event) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->NotifyAccessibilityEvent(
      event);
}

void BrowserAccessibilityManagerAuraLinux::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);

  switch (event_type) {
    case ax::mojom::Event::kScrolledToAnchor:
      ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnScrolledToAnchor();
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerAuraLinux::FireNameChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnNameChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireDescriptionChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnDescriptionChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireSortDirectionChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnSortDirectionChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireTextAttributesChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnTextAttributesChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireSubtreeCreatedEvent(
    BrowserAccessibility* node) {
  // Sending events during a load would create a lot of spam, don't do that.
  if (!GetTreeData().loaded)
    return;
  if (!CanEmitChildrenChanged(node))
    return;
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnSubtreeCreated();
}

void BrowserAccessibilityManagerAuraLinux::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);

  switch (event_type) {
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      int32_t focus_id = ax_tree()->GetUnignoredSelection().focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object)
        FireEvent(focus_object, ax::mojom::Event::kTextSelectionChanged);
      break;
    }
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireEvent(node, ax::mojom::Event::kActiveDescendantChanged);
      break;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      FireEvent(node, ax::mojom::Event::kCheckedStateChanged);
      break;
    case ui::AXEventGenerator::Event::COLLAPSED:
      FireExpandedEvent(node, false);
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      FireEvent(node, ax::mojom::Event::kDocumentTitleChanged);
      break;
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
      FireEnabledChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::EXPANDED:
      FireExpandedEvent(node, true);
      break;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      FireLoadingEvent(node, false);
      FireEvent(node, ax::mojom::Event::kLoadComplete);
      break;
    case ui::AXEventGenerator::Event::LOAD_START:
      FireLoadingEvent(node, true);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireEvent(node, ax::mojom::Event::kSelectedChildrenChanged);
      break;
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
      FireSelectedEvent(node);
      break;
    case ui::AXEventGenerator::Event::SORT_CHANGED:
      FireSortDirectionChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
      FireSubtreeCreatedEvent(node);
      break;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      FireEvent(node, ax::mojom::Event::kValueChanged);
      break;
    case ui::AXEventGenerator::Event::NAME_CHANGED:
      FireNameChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireDescriptionChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireEvent(node, ax::mojom::Event::kInvalidStatusChanged);
      break;
    case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      FireTextAttributesChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
    case ui::AXEventGenerator::Event::ALERT:
    case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::BUSY_CHANGED:
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
    case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case ui::AXEventGenerator::Event::DROPEFFECT_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
    case ui::AXEventGenerator::Event::GRABBED_CHANGED:
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
    case ui::AXEventGenerator::Event::PORTAL_ACTIVATED:
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
    case ui::AXEventGenerator::Event::VALUE_MAX_CHANGED:
    case ui::AXEventGenerator::Event::VALUE_MIN_CHANGED:
    case ui::AXEventGenerator::Event::VALUE_STEP_CHANGED:
    case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      // Need to implement.
      break;
  }
}

void BrowserAccessibilityManagerAuraLinux::OnNodeDataWillChange(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  DCHECK_EQ(ax_tree(), tree);

  // Since AuraLinux needs to send the children-changed::remove event with the
  // index in parent, the event must be fired before the node becomes ignored.
  // children-changed:add is handled with the generated Event::IGNORED_CHANGED.
  if (!old_node_data.IsIgnored() && new_node_data.IsIgnored()) {
    BrowserAccessibility* obj = GetFromID(old_node_data.id);
    if (obj && obj->GetParent()) {
      DCHECK(!obj->IsIgnored());
      if (!CanEmitChildrenChanged(obj))
        return;
      g_signal_emit_by_name(obj->GetParent(), "children-changed::remove",
                            obj->GetIndexInParent(),
                            obj->GetNativeViewAccessible());
    }
  }
}

void BrowserAccessibilityManagerAuraLinux::OnSubtreeWillBeDeleted(
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Sending events on load/destruction would create a lot of spam, avoid that.
  if (!GetTreeData().loaded)
    return;

  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!CanEmitChildrenChanged(obj))
    return;
  ToBrowserAccessibilityAuraLinux(obj)->GetNode()->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManagerAuraLinux::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  std::set<ui::AXPlatformNode*> objs_to_update;
  CollectChangedNodesAndParentsForAtomicUpdate(tree, changes, &objs_to_update);

  for (auto* node : objs_to_update)
    static_cast<ui::AXPlatformNodeAuraLinux*>(node)->UpdateHypertext();
}

void BrowserAccessibilityManagerAuraLinux::OnFindInPageResult(int request_id,
                                                              int match_index,
                                                              int start_id,
                                                              int start_offset,
                                                              int end_id,
                                                              int end_offset) {
  BrowserAccessibility* node = GetFromID(start_id);
  if (!node)
    return;
  ui::AXPlatformNodeAuraLinux* platform_node =
      ToBrowserAccessibilityAuraLinux(node)->GetNode();

  // TODO(accessibility): We should support selections that span multiple
  // elements, but for now if we see a result that spans multiple elements,
  // just activate until the end of the node.
  if (end_id != start_id)
    end_offset = platform_node->GetHypertext().size();

  platform_node->ActivateFindInPageResult(start_offset, end_offset);
}

void BrowserAccessibilityManagerAuraLinux::OnFindInPageTermination() {
  static_cast<BrowserAccessibilityAuraLinux*>(GetRoot())
      ->GetNode()
      ->TerminateFindInPage();
}

bool BrowserAccessibilityManagerAuraLinux::CanEmitChildrenChanged(
    BrowserAccessibility* node) const {
  if (!node || !ShouldFireEventForNode(node))
    return false;
  BrowserAccessibility* parent = node->PlatformGetParent();
  if (!parent || parent->PlatformIsLeaf())
    return false;
  return true;
}

}  // namespace content
