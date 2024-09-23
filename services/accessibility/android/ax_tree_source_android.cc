// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/ax_tree_source_android.h"

#include <memory>
#include <stack>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "services/accessibility/android/accessibility_node_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_window_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/auto_complete_handler.h"
#include "services/accessibility/android/drawer_layout_handler.h"
#include "services/accessibility/android/pane_title_handler.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/gfx/geometry/rect.h"

namespace ax::android {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXWindowBooleanProperty = mojom::AccessibilityWindowBooleanProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

// TODO(hirokisato): Enable AXTreeAndroidSerializer's |crash_on_error| once
// Android becomes able to send reliable trees.
AXTreeSourceAndroid::AXTreeSourceAndroid(
    Delegate* delegate,
    std::unique_ptr<SerializationDelegate> serialization_delegate,
    aura::Window* window)
    : current_tree_serializer_(
          new AXTreeAndroidSerializer(this, DCHECK_IS_ON())),
      is_notification_(false),
      is_input_method_window_(false),
      window_(window),
      delegate_(delegate),
      serialization_delegate_(std::move(serialization_delegate)) {
  CHECK(serialization_delegate_);
  serialization_delegate_->BindTree(this);
}

AXTreeSourceAndroid::~AXTreeSourceAndroid() {
  Reset();
}

void AXTreeSourceAndroid::NotifyAccessibilityEvent(AXEventData* event_data) {
  root_id_.reset();
  DCHECK(event_data);

  NotifyAccessibilityEventInternal(*event_data);

  // Clear maps in order to prevent invalid access from dead pointers.
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
}

void AXTreeSourceAndroid::NotifyActionResult(const ui::AXActionData& data,
                                             bool result) {
  GetAutomationEventRouter()->DispatchActionResult(data, result);
}

void AXTreeSourceAndroid::NotifyGetTextLocationDataResult(
    const ui::AXActionData& data,
    const std::optional<gfx::Rect>& rect) {
  GetAutomationEventRouter()->DispatchGetTextLocationDataResult(data, rect);
}

bool AXTreeSourceAndroid::UseFullFocusMode() const {
  return delegate_->UseFullFocusMode();
}

void AXTreeSourceAndroid::InvalidateTree() {
  current_tree_serializer_->Reset();
}

bool AXTreeSourceAndroid::IsRootOfNodeTree(int32_t id) const {
  const auto& node_it = tree_map_.find(id);
  if (node_it == tree_map_.end()) {
    return false;
  }

  if (!node_it->second->IsNode()) {
    return false;
  }

  const auto& parent_it = parent_map_.find(id);
  if (parent_it == parent_map_.end()) {
    return true;
  }

  const auto& parent_tree_it = tree_map_.find(parent_it->second);
  CHECK(parent_tree_it != tree_map_.end());
  return !parent_tree_it->second->IsNode();
}

void AXTreeSourceAndroid::SetVirtualNode(
    int32_t parent_id,
    std::unique_ptr<AccessibilityInfoDataWrapper> child) {
  auto* parent_node = GetFromId(parent_id);
  // TODO support any node as a parent, not limiting to a window.
  CHECK(parent_node);
  CHECK(parent_node->GetWindow());

  int32_t node_id = child->GetId();
  tree_map_[node_id] = std::move(child);
  parent_map_[node_id] = parent_node->GetId();
  static_cast<AccessibilityWindowInfoDataWrapper*>(parent_node)
      ->AddVirtualChild(node_id);
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::GetFirstImportantAncestor(
    AccessibilityInfoDataWrapper* info_data) const {
  AccessibilityInfoDataWrapper* parent = GetParent(info_data);
  while (parent && parent->IsNode() && !parent->IsImportantInAndroid()) {
    parent = GetParent(parent);
  }
  return parent;
}

AccessibilityInfoDataWrapper*
AXTreeSourceAndroid::GetFirstAccessibilityFocusableAncestor(
    AccessibilityInfoDataWrapper* info_data) const {
  AccessibilityInfoDataWrapper* node = info_data;
  while (node && !node->IsAccessibilityFocusableContainer()) {
    node = GetParent(node);
  }
  return node;
}

bool AXTreeSourceAndroid::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = ax_tree_id();
  if (android_focused_id_.has_value()) {
    data->focus_id = *android_focused_id_;
  }
  return true;
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::GetRoot() const {
  return root_id_.has_value() ? GetFromId(*root_id_) : nullptr;
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::GetParent(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data) {
    return nullptr;
  }
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end()) {
    return GetFromId(it->second);
  }
  return nullptr;
}

void AXTreeSourceAndroid::SerializeNode(AccessibilityInfoDataWrapper* info_data,
                                        ui::AXNodeData* out_data) const {
  if (!info_data) {
    return;
  }

  info_data->Serialize(out_data);

  const auto& itr = hooks_.find(info_data->GetId());
  if (itr != hooks_.end()) {
    itr->second->PostSerializeNode(out_data);
  }
}

void AXTreeSourceAndroid::BuildNodeMap(const AXEventData& event_data) {
  // Prepare the wrapper objects of mojom data from Android.
  CHECK(event_data.window_data);
  root_id_ = event_data.window_data->at(0)->window_id;
  for (const auto& window_ptr : *event_data.window_data) {
    int32_t window_id = window_ptr->window_id;
    int32_t root_node_id = window_ptr->root_node_id;
    AXWindowInfoData* window = window_ptr.get();
    if (root_node_id) {
      parent_map_[root_node_id] = window_id;
    }

    tree_map_[window_id] =
        std::make_unique<AccessibilityWindowInfoDataWrapper>(this, window);

    std::vector<int32_t> children;
    if (GetProperty(window->int_list_properties,
                    AXWindowIntListProperty::CHILD_WINDOW_IDS, &children)) {
      for (const int32_t child : children) {
        DCHECK(child != root_id_);
        parent_map_[child] = window_id;
      }
    }
  }

  for (const auto& node_ptr : event_data.node_data) {
    int32_t node_id = node_ptr->id;
    AXNodeInfoData* node = node_ptr.get();
    tree_map_[node_id] =
        std::make_unique<AccessibilityNodeInfoDataWrapper>(this, node);

    std::vector<int32_t> children;
    if (GetProperty(node_ptr.get()->int_list_properties,
                    AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children) {
        parent_map_[child] = node_id;
      }
    }
  }
}

void AXTreeSourceAndroid::NotifyAccessibilityEventInternal(
    const AXEventData& event_data) {
  if (window_id_ != event_data.window_id) {
    // Root window id is changed. Resetting variables that depends on id.
    android_focused_id_.reset();
    hooks_.clear();
    window_id_ = event_data.window_id;
  }
  is_notification_ = event_data.notification_key.has_value();
  if (is_notification_) {
    notification_key_ = event_data.notification_key;
  }
  is_input_method_window_ = event_data.is_input_method_window;

  BuildNodeMap(event_data);

  // Compute each node's bounds, based on its descendants.
  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data.node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data.node_data[i]->id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }
  for (int i = event_data.window_data->size() - 1; i >= 0; --i) {
    int32_t id = event_data.window_data->at(i)->window_id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  if (!UpdateAndroidFocusedId(event_data)) {
    // Exit this function if the focused node doesn't exist nor isn't visible.
    return;
  }

  AccessibilityInfoDataWrapper* const source_node =
      GetFromId(event_data.source_id);

  std::vector<int32_t> update_ids = ProcessHooksOnEvent(event_data);

  // Prep the event and send it to automation.
  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;
  std::vector<ui::AXEvent> events;
  const std::optional<ax::mojom::Event> event_type =
      ToAXEvent(event_data.event_type, source_node, focused_node);
  if (event_type) {
    ui::AXEvent event;
    event.event_type = event_type.value();
    event.id = event_data.source_id;

    int event_from_action;
    if (GetProperty(event_data.int_properties,
                    ax::android::mojom::AccessibilityEventIntProperty::ACTION,
                    &event_from_action)) {
      event.event_from = ax::mojom::EventFrom::kAction;

      event.event_from_action = ConvertToChromeAction(
          static_cast<mojom::AccessibilityActionType>(event_from_action));
    }

    events.push_back(std::move(event));
  }

  if (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED) {
    // On event type of WINDOW_STATE_CHANGED, update the entire tree so that
    // window location is correctly calculated.
    update_ids.push_back(*root_id_);
  } else if (!UseFullFocusMode()) {
    update_ids.push_back(event_data.source_id);
  } else {
    // Otherwise, update subtree under the event source.
    // If the event source is ignored, it's possible that the name is used by
    // ancestor.
    AccessibilityInfoDataWrapper* parent =
        GetFirstAccessibilityFocusableAncestor(source_node);
    update_ids.push_back(parent ? parent->GetId() : event_data.source_id);
  }

  for (const int32_t update_id : update_ids) {
    current_tree_serializer_->MarkSubtreeDirty(update_id);
  }

  // Serialize updates in the reverse order of |update_ids|.
  // Updates from Android event first as this contains the entire tree
  // information, including focus.
  std::vector<ui::AXTreeUpdate> updates;
  for (const int32_t update_id : base::Reversed(update_ids)) {
    AccessibilityInfoDataWrapper* update_root = GetFromId(update_id);
    if (!update_root) {
      LOG(ERROR) << "Update root node doesn't exist, id=" << update_id;
      continue;
    }
    ui::AXTreeUpdate update;
    if (!current_tree_serializer_->SerializeChanges(update_root, &update)) {
      std::string error_string;
      ui::AXTreeSourceChecker<AccessibilityInfoDataWrapper*> checker(this);
      checker.CheckAndGetErrorString(&error_string);

      LOG(ERROR) << "Unable to serialize accessibility event\n"
                 << "Error: " << error_string << "\n"
                 << "Update: " << update.ToString();
    } else {
      updates.push_back(std::move(update));
    }
  }

  GetAutomationEventRouter()->DispatchAccessibilityEvents(
      ax_tree_id(), std::move(updates), gfx::Point(), std::move(events));
}

extensions::AutomationEventRouterInterface*
AXTreeSourceAndroid::GetAutomationEventRouter() const {
  if (automation_event_router_for_test_) {
    return automation_event_router_for_test_;
  }

  return extensions::AutomationEventRouter::GetInstance();
}

gfx::Rect AXTreeSourceAndroid::ComputeEnclosingBounds(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser()) {
    return computed_bounds;
  }

  ComputeEnclosingBoundsInternal(info_data, &computed_bounds);
  return computed_bounds;
}

void AXTreeSourceAndroid::ComputeEnclosingBoundsInternal(
    AccessibilityInfoDataWrapper* info_data,
    gfx::Rect* computed_bounds) const {
  DCHECK(computed_bounds);
  auto cached_bounds = computed_bounds_.find(info_data->GetId());
  if (cached_bounds != computed_bounds_.end()) {
    computed_bounds->Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser()) {
    return;
  }
  // Only consider nodes that can possibly be accessibility focused.
  if (info_data->IsFocusableInFullFocusMode()) {
    computed_bounds->Union(info_data->GetBounds());
  }

  // NOTE: |AXTreeSourceAndroid::GetChildren| depends on ComputeEnclosingBounds.
  // To get children, directly call wrapper's GetChildren here.
  std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>
      children;
  info_data->GetChildren(&children);
  for (AccessibilityInfoDataWrapper* child : children) {
    ComputeEnclosingBoundsInternal(child, computed_bounds);
  }
  return;
}

AccessibilityInfoDataWrapper*
AXTreeSourceAndroid::FindFirstFocusableNodeInFullFocusMode(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data) {
    return nullptr;
  }

  if (info_data->IsVisibleToUser() && info_data->IsFocusableInFullFocusMode()) {
    return info_data;
  }

  for (AccessibilityInfoDataWrapper* child : GetChildren(info_data)) {
    AccessibilityInfoDataWrapper* candidate =
        FindFirstFocusableNodeInFullFocusMode(child);
    if (candidate) {
      return candidate;
    }
  }

  return nullptr;
}

bool AXTreeSourceAndroid::UpdateAndroidFocusedId(
    const AXEventData& event_data) {
  AccessibilityInfoDataWrapper* source_node = GetFromId(event_data.source_id);
  if (source_node) {
    AccessibilityInfoDataWrapper* source_window =
        GetFromId(source_node->GetWindowId());
    if (!source_window ||
        !GetBooleanProperty(source_window->GetWindow(),
                            AXWindowBooleanProperty::FOCUSED)) {
      // Don't update focus in this task for events from non-focused window.
      return true;
    }
  }

  if (event_data.event_type == AXEventType::VIEW_FOCUSED) {
    if (source_node && source_node->IsVisibleToUser() &&
        GetBooleanProperty(source_node->GetNode(),
                           AXBooleanProperty::FOCUSED)) {
      // Sometimes Android sets focus on unfocusable node, e.g. ListView.
      AccessibilityInfoDataWrapper* adjusted_node =
          UseFullFocusMode()
              ? FindFirstFocusableNodeInFullFocusMode(source_node)
              : source_node;
      if (adjusted_node) {
        android_focused_id_ = adjusted_node->GetId();
      }
    }
  } else if (event_data.event_type == AXEventType::VIEW_ACCESSIBILITY_FOCUSED &&
             UseFullFocusMode()) {
    if (source_node && source_node->IsVisibleToUser()) {
      android_focused_id_ = source_node->GetId();
    }
  } else if (event_data.event_type ==
                 AXEventType::VIEW_ACCESSIBILITY_FOCUS_CLEARED &&
             UseFullFocusMode()) {
    int event_from_action;
    GetProperty(event_data.int_properties,
                mojom::AccessibilityEventIntProperty::ACTION,
                &event_from_action);
    const mojom::AccessibilityActionType action =
        static_cast<mojom::AccessibilityActionType>(event_from_action);
    if (action != mojom::AccessibilityActionType::FOCUS &&
        action != mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS) {
      android_focused_id_.reset();
    }
  } else if (event_data.event_type == AXEventType::VIEW_SELECTED) {
    // In Android, VIEW_SELECTED event is dispatched in the two cases below:
    // 1. Changing a value in ProgressBar or TimePicker in Android P.
    // 2. Selecting an item in the context of an AdapterView.
    if (!source_node || !source_node->IsNode()) {
      return false;
    }

    AXNodeInfoData* node_info = source_node->GetNode();
    DCHECK(node_info);

    bool is_range_change = !node_info->range_info.is_null();
    if (!is_range_change) {
      AccessibilityInfoDataWrapper* selected_node =
          GetSelectedNodeInfoFromAdapterViewEvent(event_data, source_node);
      if (!selected_node || !selected_node->IsVisibleToUser()) {
        return false;
      }

      android_focused_id_ = selected_node->GetId();
    }
  } else if (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED) {
    // When accessibility window changed, a11y event of WINDOW_CONTENT_CHANGED
    // is fired from Android multiple times.
    // The event of WINDOW_STATE_CHANGED is fired only once for each window
    // change and use it as a trigger to move the a11y focus to the first node.
    AccessibilityInfoDataWrapper* new_focus = nullptr;

    // If the current window has ever been visited in the current task, try
    // focus on the last focus node in this window.
    // We do it for WINDOW_STATE_CHANGED event from a window or a root node.
    bool from_root_or_window = (source_node && !source_node->IsNode()) ||
                               IsRootOfNodeTree(event_data.source_id);
    if (from_root_or_window) {
      auto itr = window_id_to_last_focus_node_id_.find(event_data.window_id);
      if (itr != window_id_to_last_focus_node_id_.end()) {
        new_focus = GetFromId(itr->second);
      }
    } else if (UseFullFocusMode()) {
      // Otherwise, try focus on the first focusable node.
      new_focus = FindFirstFocusableNodeInFullFocusMode(
          GetFromId(event_data.source_id));
    }

    if (new_focus) {
      android_focused_id_ = new_focus->GetId();
    }
  }

  if (!android_focused_id_ || !GetFromId(*android_focused_id_)) {
    // Because we only handle events from the focused window, let's reset the
    // focus to the root window.
    AccessibilityInfoDataWrapper* root = GetRoot();
    CHECK(root);
    android_focused_id_ = root_id_;
  }

  if (android_focused_id_.has_value()) {
    window_id_to_last_focus_node_id_[event_data.window_id] =
        *android_focused_id_;
  } else {
    window_id_to_last_focus_node_id_.erase(event_data.window_id);
  }

  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;

  // Ensure that the focused node correctly gets focus.
  while (focused_node && focused_node->IsIgnored()) {
    AccessibilityInfoDataWrapper* parent = GetParent(focused_node);
    if (parent) {
      android_focused_id_ = parent->GetId();
      focused_node = parent;
    } else {
      // Unable to find the appropriate focus. Removing the focused node.
      android_focused_id_.reset();
      focused_node = nullptr;
      break;
    }
  }

  return true;
}

std::vector<int32_t> AXTreeSourceAndroid::ProcessHooksOnEvent(
    const AXEventData& event_data) {
  base::EraseIf(hooks_, [this](const auto& it) {
    return it.second->ShouldDestroy(this);
  });

  std::vector<int32_t> serialization_needed_ids;
  for (const auto& modifier : hooks_) {
    if (modifier.second->PreDispatchEvent(this, event_data)) {
      serialization_needed_ids.push_back(modifier.first);
    }
  }

  // Add new hook implementations if necessary.
  auto drawer_layout_hook =
      DrawerLayoutHandler::CreateIfNecessary(this, event_data);
  if (drawer_layout_hook.has_value()) {
    hooks_.insert(std::move(*drawer_layout_hook));
  }

  auto auto_complete_hooks =
      AutoCompleteHandler::CreateIfNecessary(this, event_data);
  for (auto& modifier : auto_complete_hooks) {
    if (hooks_.count(modifier.first) == 0) {
      hooks_.insert(std::move(modifier));
    }
  }

  auto pane_title_hook = PaneTitleHandler::CreateIfNecessary(this, event_data);
  if (pane_title_hook) {
    hooks_.insert(std::move(*pane_title_hook));
  }

  return serialization_needed_ids;
}

void AXTreeSourceAndroid::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
  current_tree_serializer_ = std::make_unique<AXTreeAndroidSerializer>(this);
  root_id_.reset();
  window_id_.reset();
  android_focused_id_.reset();
  extensions::AutomationEventRouterInterface* router =
      GetAutomationEventRouter();
  if (!router) {
    return;
  }

  router->DispatchTreeDestroyedEvent(ax_tree_id());
}

bool AXTreeSourceAndroid::NeedReorder(
    AccessibilityInfoDataWrapper* left,
    AccessibilityInfoDataWrapper* right) const {
  auto left_bounds = ComputeEnclosingBounds(left);
  auto right_bounds = ComputeEnclosingBounds(right);
  return !CompareBounds(left_bounds, right_bounds) &&
         CompareBounds(left->GetBounds(), right->GetBounds());
}

bool AXTreeSourceAndroid::CompareBounds(const gfx::Rect& left,
                                        const gfx::Rect& right) const {
  if (left.IsEmpty() || right.IsEmpty()) {
    return true;
  }

  // Non-intersecting vertical check.
  if (left.bottom() <= right.y()) {
    return true;
  }

  if (left.y() >= right.bottom()) {
    return false;
  }

  // Vertically overlapping. Left one comes first.
  // TODO consider right-to-left
  int left_difference = left.x() - right.x();
  if (left_difference != 0) {
    return left_difference < 0;
  }

  // Top to bottom.
  int top_difference = left.y() - right.y();
  if (top_difference != 0) {
    return top_difference < 0;
  }

  // Larger to smaller.
  int height_difference = left.height() - right.height();
  if (height_difference != 0) {
    return height_difference > 0;
  }

  int width_difference = left.width() - right.width();
  if (width_difference != 0) {
    return width_difference > 0;
  }

  // The rects are equal. Respect the original order.
  return true;
}

int32_t AXTreeSourceAndroid::GetId(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data) {
    return ui::kInvalidAXNodeID;
  }
  return info_data->GetId();
}

void AXTreeSourceAndroid::CacheChildrenIfNeeded(
    AccessibilityInfoDataWrapper* info_data) {
  ComputeAndCacheChildren(info_data);
}

size_t AXTreeSourceAndroid::GetChildCount(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data) {
    return 0;
  }
  DCHECK(info_data->cached_children_);
  return info_data->cached_children_->size();
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::ChildAt(
    AccessibilityInfoDataWrapper* info_data,
    size_t i) const {
  DCHECK(info_data->cached_children_);
  DCHECK(i >= 0 && i < info_data->cached_children_->size());
  return (*info_data->cached_children_)[i];
}

// We don't need to handle cache clearing here, because each
// AccessibilityInfoDataWrapper is created during
// AXTreeSourceAndroid::NotifyAccessibilityEvent(), and destructed at the end of
// it that method.
void AXTreeSourceAndroid::ClearChildCache(
    AccessibilityInfoDataWrapper* info_data) {}

bool AXTreeSourceAndroid::IsIgnored(
    AccessibilityInfoDataWrapper* info_data) const {
  return false;
}

bool AXTreeSourceAndroid::IsEqual(
    AccessibilityInfoDataWrapper* info_data1,
    AccessibilityInfoDataWrapper* info_data2) const {
  if (!info_data1 || !info_data2) {
    return false;
  }
  return info_data1->GetId() == info_data2->GetId();
}

AccessibilityInfoDataWrapper* AXTreeSourceAndroid::GetNull() const {
  return nullptr;
}

void AXTreeSourceAndroid::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>&
AXTreeSourceAndroid::GetChildren(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  ComputeAndCacheChildren(info_data);
  return info_data->cached_children_.value();
}

void AXTreeSourceAndroid::ComputeAndCacheChildren(
    AccessibilityInfoDataWrapper* info_data) const {
  if (info_data->cached_children_) {
    return;
  }

  std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>&
      children = info_data->cached_children_.emplace();

  info_data->GetChildren(&children);
  if (children.size() < 2) {
    return;
  }

  // We sort output nodes only in full focus mode.
  if (!UseFullFocusMode() || info_data->IsWebNode()) {
    return;
  }

  // Also don't sort for virtual nodes (e.g. WebView).
  for (const AccessibilityInfoDataWrapper* child : children) {
    if (child->IsWebNode()) {
      return;
    }
  }

  // This is a kind of bubble sort, but we reorder nodes only when the original
  // node bounds (which is from node->GetBounds()) and child enclosing bounds
  // (which is from ComputeEnclosingBounds()) are different.
  // This algorithm takes O(N^2) time, but we practically don't expect that
  // there's a node that contains hundreds of child nodes that require
  // reordering.
  //
  // The concept here is taken from Android accessibility's similar logic in
  // com.google.android.accessibility.utils.traversal.ReorderedChildrenIterator.
  //
  // Note that NeedReorder method is not transitive, so we cannot sort with it.
  // For example, consider bounds below:
  //   a = (0,11)-(10x10)
  //   b = (20,5)-(10x10)
  //   c = (40,0)-(10x10)
  // Here, NeedReorder(a, b) = false, NeedReorder(b, c) = false, but
  // NeedReorder(a, c) = true.

  for (int i = children.size() - 2; i >= 0; i--) {
    auto original_bounds = children.at(i)->GetBounds();
    auto enclosing_bounds = ComputeEnclosingBounds(children.at(i));
    if (original_bounds == enclosing_bounds) {
      continue;
    }

    // move the current node to be visited later if necessary.
    for (size_t j = i; j + 1 < children.size() &&
                       NeedReorder(children.at(j), children.at(j + 1));
         j++) {
      std::swap(children.at(j), children.at(j + 1));
    }
  }
}

}  // namespace ax::android
