// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_auralinux.h"

#include <vector>

#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "content/common/accessibility_messages.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAuraLinux(nullptr, initial_tree,
                                                  delegate, factory);
}

BrowserAccessibilityManagerAuraLinux*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAuraLinux() {
  return static_cast<BrowserAccessibilityManagerAuraLinux*>(this);
}

BrowserAccessibilityManagerAuraLinux::BrowserAccessibilityManagerAuraLinux(
    AtkObject* parent_object,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      parent_object_(parent_object) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerAuraLinux::~BrowserAccessibilityManagerAuraLinux() {
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerAuraLinux::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
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
  if (!node->IsNative())
    return;

  gfx::NativeViewAccessible obj = node->GetNativeViewAccessible();
  if (!ATK_IS_OBJECT(obj))
    return;

  atk_object_notify_state_change(obj, ATK_STATE_BUSY, is_loading);
  if (!is_loading)
    g_signal_emit_by_name(obj, "load_complete");
}

void BrowserAccessibilityManagerAuraLinux::FireExpandedEvent(
    BrowserAccessibility* node,
    bool is_expanded) {
  if (!node->IsNative())
    return;

  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnExpandedStateChanged(
      is_expanded);
}

void BrowserAccessibilityManagerAuraLinux::FireEvent(BrowserAccessibility* node,
                                                     ax::mojom::Event event) {
  if (!node->IsNative())
    return;

  ToBrowserAccessibilityAuraLinux(node)->GetNode()->NotifyAccessibilityEvent(
      event);
}

void BrowserAccessibilityManagerAuraLinux::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  // Need to implement.
}

void BrowserAccessibilityManagerAuraLinux::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);

  switch (event_type) {
    case Event::CHECKED_STATE_CHANGED:
      FireEvent(node, ax::mojom::Event::kCheckedStateChanged);
      break;
    case Event::COLLAPSED:
      FireExpandedEvent(node, false);
      break;
    case Event::EXPANDED:
      FireExpandedEvent(node, true);
      break;
    case Event::LOAD_COMPLETE:
      FireLoadingEvent(node, false);
      break;
    case Event::LOAD_START:
      FireLoadingEvent(node, true);
      break;
    case Event::MENU_ITEM_SELECTED:
    case Event::SELECTED_CHANGED:
      FireSelectedEvent(node);
      break;
    case Event::VALUE_CHANGED:
      FireEvent(node, ax::mojom::Event::kValueChanged);
      break;
    default:
      // Need to implement.
      break;
  }
}

void BrowserAccessibilityManagerAuraLinux::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  // This is the second step in what will be a three step process mirroring that
  // used in BrowserAccessibilityManagerWin.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative())
      ToBrowserAccessibilityAuraLinux(obj)->GetNode()->UpdateHypertext();
  }
}

}  // namespace content
