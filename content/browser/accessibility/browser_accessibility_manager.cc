// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace content {

namespace {
// A function to call when focus changes, for testing only.
base::LazyInstance<base::RepeatingClosure>::DestructorAtExit
    g_focus_change_callback_for_testing = LAZY_INSTANCE_INITIALIZER;

// If 2 or more tree updates can all be merged into others,
// process the whole set of tree updates, copying them to |dst|,
// and returning true.  Otherwise, return false and |dst|
// is left unchanged.
//
// Merging tree updates helps minimize the overhead of calling
// Unserialize multiple times.
bool MergeTreeUpdates(const std::vector<ui::AXTreeUpdate>& src,
                      std::vector<ui::AXTreeUpdate>* dst) {
  size_t merge_count = 0;
  for (size_t i = 1; i < src.size(); i++) {
    if (ui::TreeUpdatesCanBeMerged(src[i - 1], src[i]))
      merge_count++;
  }

  // Doing a single merge isn't necessarily worth it because
  // copying the tree updates takes time too so the total
  // savings is less. But two more more merges is probably
  // worth the overhead of copying.
  if (merge_count < 2)
    return false;

  dst->resize(src.size() - merge_count);
  (*dst)[0] = src[0];
  size_t dst_index = 0;
  for (size_t i = 1; i < src.size(); i++) {
    if (ui::TreeUpdatesCanBeMerged(src[i - 1], src[i])) {
      std::vector<ui::AXNodeData>& dst_nodes = (*dst)[dst_index].nodes;
      const std::vector<ui::AXNodeData>& src_nodes = src[i].nodes;
      dst_nodes.insert(dst_nodes.end(), src_nodes.begin(), src_nodes.end());
    } else {
      dst_index++;
      (*dst)[dst_index] = src[i];
    }
  }

  return true;
}

}  // namespace

ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node1,
    const ui::AXNodeData& node2 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node3 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node4 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node5 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node6 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node7 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node8 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node9 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node10 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node11 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node12 /* = ui::AXNodeData() */) {
  static base::NoDestructor<ui::AXNodeData> empty_data;
  int32_t no_id = empty_data->id;

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = node1.id;
  update.nodes.push_back(node1);
  if (node2.id != no_id)
    update.nodes.push_back(node2);
  if (node3.id != no_id)
    update.nodes.push_back(node3);
  if (node4.id != no_id)
    update.nodes.push_back(node4);
  if (node5.id != no_id)
    update.nodes.push_back(node5);
  if (node6.id != no_id)
    update.nodes.push_back(node6);
  if (node7.id != no_id)
    update.nodes.push_back(node7);
  if (node8.id != no_id)
    update.nodes.push_back(node8);
  if (node9.id != no_id)
    update.nodes.push_back(node9);
  if (node10.id != no_id)
    update.nodes.push_back(node10);
  if (node11.id != no_id)
    update.nodes.push_back(node11);
  if (node12.id != no_id)
    update.nodes.push_back(node12);
  return update;
}

BrowserAccessibilityFindInPageInfo::BrowserAccessibilityFindInPageInfo()
    : request_id(-1),
      match_index(-1),
      start_id(-1),
      start_offset(0),
      end_id(-1),
      end_offset(-1),
      active_request_id(-1) {}

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManager(initial_tree, delegate);
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::FromID(
    ui::AXTreeID ax_tree_id) {
  return static_cast<BrowserAccessibilityManager*>(
      ui::AXTreeManagerMap::GetInstance().GetManager(ax_tree_id));
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate)
    : WebContentsObserver(delegate ? delegate->AccessibilityWebContents()
                                   : nullptr),
      delegate_(delegate),
      user_is_navigating_away_(false),
      connected_to_parent_tree_node_(false),
      ax_tree_id_(ui::AXTreeIDUnknown()),
      device_scale_factor_(1.0f),
      use_custom_device_scale_factor_for_testing_(false),
      tree_(std::make_unique<ui::AXSerializableTree>()),
      event_generator_(ax_tree()) {
  tree_observer_.Add(ax_tree());
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate)
    : WebContentsObserver(delegate ? delegate->AccessibilityWebContents()
                                   : nullptr),
      delegate_(delegate),
      user_is_navigating_away_(false),
      ax_tree_id_(ui::AXTreeIDUnknown()),
      device_scale_factor_(1.0f),
      use_custom_device_scale_factor_for_testing_(false),
      tree_(std::make_unique<ui::AXSerializableTree>()),
      event_generator_(ax_tree()) {
  tree_observer_.Add(ax_tree());
  Initialize(initial_tree);
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  // Fire any events that need to be fired when tree nodes get deleted. For
  // example, events that fire every time "OnSubtreeWillBeDeleted" is called.
  ax_tree()->Destroy();
  delegate_ = nullptr;  // Guard against reentrancy by screen reader.
  if (last_focused_node_tree_id_ &&
      ax_tree_id_ == *last_focused_node_tree_id_) {
    SetLastFocusedNode(nullptr);
  }

  ui::AXTreeManagerMap::GetInstance().RemoveTreeManager(ax_tree_id_);
}

bool BrowserAccessibilityManager::Unserialize(
    const ui::AXTreeUpdate& tree_update) {
  if (ax_tree()->Unserialize(tree_update))
    return true;

  LOG(ERROR) << ax_tree()->error();
  LOG(ERROR) << tree_update.ToString();

  static auto* ax_tree_error = base::debug::AllocateCrashKeyString(
      "ax_tree_error", base::debug::CrashKeySize::Size256);
  static auto* ax_tree_update = base::debug::AllocateCrashKeyString(
      "ax_tree_update", base::debug::CrashKeySize::Size256);
  // Temporarily log some additional crash keys so we can try to
  // figure out why we're getting bad accessibility trees here.
  // http://crbug.com/765490, https://crbug.com/1094848.
  // Be sure to re-enable BrowserAccessibilityManagerTest.TestFatalError
  // when done (or delete it if no longer needed).
  base::debug::SetCrashKeyString(ax_tree_error, ax_tree()->error());
  base::debug::SetCrashKeyString(ax_tree_update, tree_update.ToString());
  return false;
}

void BrowserAccessibilityManager::Initialize(
    const ui::AXTreeUpdate& initial_tree) {
  if (!Unserialize(initial_tree))
    LOG(FATAL) << ax_tree()->error();
}

// A flag for use in tests to ensure events aren't suppressed or delayed.
// static
bool BrowserAccessibilityManager::never_suppress_or_delay_events_for_testing_ =
    false;

// static
base::Optional<int32_t> BrowserAccessibilityManager::last_focused_node_id_ = {};

// static
base::Optional<ui::AXTreeID>
    BrowserAccessibilityManager::last_focused_node_tree_id_ = {};

// static
ui::AXTreeUpdate BrowserAccessibilityManager::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 1;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManager::FireFocusEventsIfNeeded() {
  BrowserAccessibility* focus = GetFocus();
  // If |focus| is nullptr it means that we have no way of knowing where the
  // focus is.
  //
  // One case when this would happen is when the current tree hasn't connected
  // to its parent tree yet. That would mean that we have no way of getting to
  // the top document which holds global focus information for the whole page.
  //
  // Note that if there is nothing focused on the page, then the focus should
  // not be nullptr. The rootnode of the top document should be focused instead.
  if (!focus)
    return;

  // Don't fire focus events if the window itself doesn't have focus.
  // Bypass this check for some tests.
  if (!never_suppress_or_delay_events_for_testing_ &&
      !g_focus_change_callback_for_testing.Get()) {
    if (delegate_ && !delegate_->AccessibilityViewHasFocus())
      return;
    if (!CanFireEvents())
      return;
  }

  // Don't allow the top document to be focused if it has no children and hasn't
  // finished loading yet. Wait for at least a tiny bit of content, or for the
  // document to actually finish loading.
  // Even after the document has loaded, we shouldn't fire a focus event if the
  // document is completely empty, otherwise the user will be placed inside an
  // empty container. This would result in user confusion, since none of the
  // screen reader commands will read anything.
  if (focus == focus->manager()->GetRoot() &&
      (focus->PlatformChildCount() == 0 ||
       !focus->manager()->GetTreeData().loaded)) {
    return;
  }

  // Wait until navigation is complete or stopped, before attempting to move the
  // accessibility focus.
  if (user_is_navigating_away_)
    return;

  BrowserAccessibility* last_focused_node = GetLastFocusedNode();
  if (focus != last_focused_node)
    FireFocusEvent(focus);
  SetLastFocusedNode(focus);
}

bool BrowserAccessibilityManager::CanFireEvents() const {
  return true;
}

BrowserAccessibility* BrowserAccessibilityManager::RetargetForEvents(
    BrowserAccessibility* node,
    RetargetEventType type) const {
  return node;
}

void BrowserAccessibilityManager::FireFocusEvent(BrowserAccessibility* node) {
  if (g_focus_change_callback_for_testing.Get())
    g_focus_change_callback_for_testing.Get().Run();
}

void BrowserAccessibilityManager::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  if (!generated_event_callback_for_testing_.is_null()) {
    generated_event_callback_for_testing_.Run(delegate(), event_type,
                                              node->GetId());
  }
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() const {
  ui::AXNode* root = GetRootAsAXNode();
  return root ? GetFromAXNode(root) : nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromAXNode(
    const ui::AXNode* node) const {
  if (!node)
    return nullptr;
  return GetFromID(node->id());
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromID(int32_t id) const {
  const auto iter = id_wrapper_map_.find(id);
  if (iter != id_wrapper_map_.end())
    return iter->second;

  return nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::GetParentNodeFromParentTree()
    const {
  ui::AXNode* parent = GetParentNodeFromParentTreeAsAXNode();
  ui::AXTreeID parent_tree_id = GetParentTreeID();
  BrowserAccessibilityManager* parent_manager =
      BrowserAccessibilityManager::FromID(parent_tree_id);
  return parent && parent_manager ? parent_manager->GetFromAXNode(parent)
                                  : nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::GetPopupRoot() const {
  DCHECK(popup_root_ids_.size() <= 1);
  if (popup_root_ids_.size() == 1) {
    BrowserAccessibility* node = GetFromID(*popup_root_ids_.begin());
    if (node) {
      DCHECK(node->GetData().role == ax::mojom::Role::kRootWebArea);
      return node;
    }
  }
  return nullptr;
}

const ui::AXTreeData& BrowserAccessibilityManager::GetTreeData() const {
  return ax_tree()->data();
}

void BrowserAccessibilityManager::OnWindowFocused() {
  if (IsRootTree())
    FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::OnWindowBlurred() {
  if (IsRootTree())
    SetLastFocusedNode(nullptr);
}

void BrowserAccessibilityManager::UserIsNavigatingAway() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::UserIsReloading() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::NavigationSucceeded() {
  user_is_navigating_away_ = false;
  FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::NavigationFailed() {
  user_is_navigating_away_ = false;
  FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::DidStopLoading() {
  user_is_navigating_away_ = false;
  FireFocusEventsIfNeeded();
}

bool BrowserAccessibilityManager::UseRootScrollOffsetsWhenComputingBounds() {
  return use_root_scroll_offsets_when_computing_bounds_;
}

void BrowserAccessibilityManager ::
    SetUseRootScrollOffsetsWhenComputingBoundsForTesting(bool use) {
  use_root_scroll_offsets_when_computing_bounds_ = use;
}

bool BrowserAccessibilityManager::OnAccessibilityEvents(
    const AXEventNotificationDetails& details) {
  TRACE_EVENT0("accessibility",
               "BrowserAccessibilityManager::OnAccessibilityEvents");

  // Update the cached device scale factor.
  if (delegate_ && !use_custom_device_scale_factor_for_testing_)
    device_scale_factor_ = delegate_->AccessibilityGetDeviceScaleFactor();

  // Optionally merge multiple tree updates into fewer updates.
  const std::vector<ui::AXTreeUpdate>* tree_updates = &details.updates;
  std::vector<ui::AXTreeUpdate> merged_tree_updates;
  if (MergeTreeUpdates(details.updates, &merged_tree_updates))
    tree_updates = &merged_tree_updates;

  // Process all changes to the accessibility tree first.
  for (const ui::AXTreeUpdate& tree_update : *tree_updates) {
    if (!Unserialize(tree_update)) {
      // This is a fatal error, but if there is a delegate, it will handle the
      // error result and recover by re-creating the manager. After a max
      // threshold number of errors is reached, it will crash the browser.
      if (!delegate_)
        CHECK(false) << ax_tree()->error();
      return false;
    }

    // It's a bug if we got an update containing more nodes than
    // the size of the resulting tree. If Unserialize succeeded that
    // means a node just got repeated or something harmless like that,
    // but it should still be investigated and could be the sign of a
    // performance issue.
    DCHECK_LE(int{tree_update.nodes.size()}, ax_tree()->size());
  }

  // If this page is hidden by an interstitial, suppress all events.
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager && root_manager->hidden_by_interstitial_page()) {
    event_generator().ClearEvents();
    return true;
  }

  // Allow derived classes to do event pre-processing.
  BeforeAccessibilityEvents();

  // If the root's parent is in another accessibility tree but it wasn't
  // previously connected, post the proper notifications on the parent.
  BrowserAccessibility* parent = GetParentNodeFromParentTree();
  if (parent) {
    if (!connected_to_parent_tree_node_) {
      parent->OnDataChanged();
      parent->UpdatePlatformAttributes();
      parent = RetargetForEvents(parent,
                                 RetargetEventType::RetargetEventTypeGenerated);
      FireGeneratedEvent(ui::AXEventGenerator::Event::CHILDREN_CHANGED, parent);
      connected_to_parent_tree_node_ = true;
    }
  } else {
    connected_to_parent_tree_node_ = false;
  }

  // Based on the changes to the tree, fire focus events if needed.
  // Screen readers might not do the right thing if they're not aware of what
  // has focus, so always try that first. Nothing will be fired if the window
  // itself isn't focused or if focus hasn't changed.
  //
  // We need to fire focus events specifically from the root manager, since we
  // need the top document's delegate to check if its view has focus.
  //
  // If this manager is disconnected from the top document, then root_manager
  // will be a null pointer and FireFocusEventsIfNeeded won't be able to
  // retrieve the global focus (not firing an event anyway).
  if (root_manager)
    root_manager->FireFocusEventsIfNeeded();

  bool received_load_complete_event = false;
  // Fire any events related to changes to the tree.
  for (const auto& targeted_event : event_generator()) {
    BrowserAccessibility* event_target = GetFromAXNode(targeted_event.node);
    if (!event_target)
      continue;

    event_target = RetargetForEvents(
        event_target, RetargetEventType::RetargetEventTypeGenerated);
    if (!event_target || !event_target->CanFireEvents())
      continue;

    if (targeted_event.event_params.event ==
        ui::AXEventGenerator::Event::LOAD_COMPLETE) {
      received_load_complete_event = true;
    }

    FireGeneratedEvent(targeted_event.event_params.event, event_target);
  }
  event_generator().ClearEvents();

  // Fire events from Blink.
  for (const ui::AXEvent& event : details.events) {
    // Fire the native event.
    BrowserAccessibility* event_target = GetFromID(event.id);
    if (!event_target)
      continue;
    RetargetEventType type =
        event.event_type == ax::mojom::Event::kHover
            ? RetargetEventType::RetargetEventTypeBlinkHover
            : RetargetEventType::RetargetEventTypeBlinkGeneral;
    BrowserAccessibility* retargeted = RetargetForEvents(event_target, type);
    if (!retargeted || !retargeted->CanFireEvents())
      continue;

    if (root_manager && event.event_type == ax::mojom::Event::kHover)
      root_manager->CacheHitTestResult(event_target);

    FireBlinkEvent(event.event_type, retargeted);
  }

  if (received_load_complete_event) {
    // Fire a focus event after the document has finished loading, but after all
    // the platform independent events have already fired, e.g. kLayoutComplete.
    // Some screen readers need a focus event in order to work properly.
    FireFocusEventsIfNeeded();

    // Perform the initial run of language detection.
    ax_tree()->language_detection_manager->DetectLanguages();
    ax_tree()->language_detection_manager->LabelLanguages();

    // After initial language detection, enable language detection for future
    // content updates in order to support dynamic content changes.
    //
    // If the LanguageDetectionDynamic feature flag is not enabled then this
    // is a no-op.
    ax_tree()->language_detection_manager->RegisterLanguageDetectionObserver();
  }

  // Allow derived classes to do event post-processing.
  FinalizeAccessibilityEvents();
  return true;
}

void BrowserAccessibilityManager::BeforeAccessibilityEvents() {}

void BrowserAccessibilityManager::FinalizeAccessibilityEvents() {}

void BrowserAccessibilityManager::OnLocationChanges(
    const std::vector<mojom::LocationChangesPtr>& changes) {
  for (auto& change : changes) {
    BrowserAccessibility* obj = GetFromID(change->id);
    if (!obj)
      continue;
    ui::AXNode* node = obj->node();
    node->SetLocation(change->new_location.offset_container_id,
                      change->new_location.bounds,
                      change->new_location.transform.get());
  }
  SendLocationChangeEvents(changes);
}

void BrowserAccessibilityManager::SendLocationChangeEvents(
    const std::vector<mojom::LocationChangesPtr>& changes) {
  for (auto& change : changes) {
    BrowserAccessibility* obj = GetFromID(change->id);
    if (obj)
      obj->OnLocationChanged();
  }
}

void BrowserAccessibilityManager::OnFindInPageResult(int request_id,
                                                     int match_index,
                                                     int start_id,
                                                     int start_offset,
                                                     int end_id,
                                                     int end_offset) {
  find_in_page_info_.request_id = request_id;
  find_in_page_info_.match_index = match_index;
  find_in_page_info_.start_id = start_id;
  find_in_page_info_.start_offset = start_offset;
  find_in_page_info_.end_id = end_id;
  find_in_page_info_.end_offset = end_offset;

  if (find_in_page_info_.active_request_id == request_id)
    ActivateFindInPageResult(request_id);
}

void BrowserAccessibilityManager::ActivateFindInPageResult(int request_id) {
  find_in_page_info_.active_request_id = request_id;
  if (find_in_page_info_.request_id != request_id)
    return;

  BrowserAccessibility* node = GetFromID(find_in_page_info_.start_id);
  if (!node)
    return;

  // If an ancestor of this node is a leaf node, fire the notification on that.
  node = node->PlatformGetClosestPlatformObject();

  // The "scrolled to anchor" notification is a great way to get a
  // screen reader to jump directly to a specific location in a document.
  FireBlinkEvent(ax::mojom::Event::kScrolledToAnchor, node);
}

BrowserAccessibility* BrowserAccessibilityManager::GetActiveDescendant(
    BrowserAccessibility* focus) const {
  if (!focus)
    return nullptr;

  int32_t active_descendant_id;
  BrowserAccessibility* active_descendant = nullptr;
  if (focus->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                             &active_descendant_id)) {
    active_descendant = focus->manager()->GetFromID(active_descendant_id);
  }

  if (focus->GetRole() == ax::mojom::Role::kPopUpButton) {
    BrowserAccessibility* child = focus->InternalGetFirstChild();
    if (child && child->GetRole() == ax::mojom::Role::kMenuListPopup &&
        !child->GetData().HasState(ax::mojom::State::kInvisible)) {
      // The active descendant is found on the menu list popup, i.e. on the
      // actual list and not on the button that opens it.
      // If there is no active descendant, focus should stay on the button so
      // that Windows screen readers would enable their virtual cursor.
      // Do not expose an activedescendant in a hidden/collapsed list, as
      // screen readers expect the focus event to go to the button itself.
      // Note that the AX hierarchy in this case is strange -- the active
      // option is the only visible option, and is inside an invisible list.
      if (child->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                 &active_descendant_id)) {
        active_descendant = child->manager()->GetFromID(active_descendant_id);
      }
    }
  }

  if (active_descendant &&
      !active_descendant->GetData().HasState(ax::mojom::State::kInvisible))
    return active_descendant;

  return focus;
}

bool BrowserAccessibilityManager::NativeViewHasFocus() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  return delegate && delegate->AccessibilityViewHasFocus();
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus() const {
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (!root_manager) {
    // We can't retrieved the globally focused object since we don't have access
    // to the top document. If we return the focus in the current or a
    // descendent tree, it might be wrong, since the top document might have
    // another frame as the tree with the focus.
    return nullptr;
  }

  ui::AXTreeID focused_tree_id = root_manager->GetTreeData().focused_tree_id;
  BrowserAccessibilityManager* focused_manager = nullptr;
  if (focused_tree_id != ui::AXTreeIDUnknown())
    focused_manager = BrowserAccessibilityManager::FromID(focused_tree_id);

  // BrowserAccessibilityManager::FromID(focused_tree_id) may return nullptr if
  // the tree is not created or has been destroyed. In this case, we don't
  // really know where the focus is, so we should return nullptr. However, due
  // to a bug in RenderFrameHostImpl this is currently not possible.
  //
  // TODO(nektar): Fix All the issues identified in crbug.com/956748
  if (!focused_manager)
    return GetFocusFromThisOrDescendantFrame();

  return focused_manager->GetFocusFromThisOrDescendantFrame();
}

BrowserAccessibility*
BrowserAccessibilityManager::GetFocusFromThisOrDescendantFrame() const {
  int32_t focus_id = GetTreeData().focus_id;
  BrowserAccessibility* obj = GetFromID(focus_id);
  // If nothing is focused, then the top document has the focus.
  if (!obj)
    return GetRoot();

  if (obj->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        obj->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    const BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(child_tree_id);
    if (child_manager)
      return child_manager->GetFocusFromThisOrDescendantFrame();
  }

  return obj;
}

void BrowserAccessibilityManager::SetFocus(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.SetFocus"));

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = node.GetId();
  if (!delegate_->AccessibilityViewHasFocus())
    delegate_->AccessibilityViewSetFocus();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetSequentialFocusNavigationStartingPoint(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action =
      ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetFocusLocallyForTesting(
    BrowserAccessibility* node) {
  ui::AXTreeData data = GetTreeData();
  data.focus_id = node->GetId();
  ax_tree()->UpdateData(data);
}

// static
void BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
    base::RepeatingClosure callback) {
  g_focus_change_callback_for_testing.Get() = std::move(callback);
}

void BrowserAccessibilityManager::SetGeneratedEventCallbackForTesting(
    const GeneratedEventCallbackForTesting& callback) {
  generated_event_callback_for_testing_ = callback;
}

// static
void BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting() {
  never_suppress_or_delay_events_for_testing_ = true;
}

void BrowserAccessibilityManager::Decrement(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDecrement;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.DoDefault"));

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::GetImageData(const BrowserAccessibility& node,
                                               const gfx::Size& max_size) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kGetImageData;
  action_data.target_node_id = node.GetId();
  action_data.target_rect = gfx::Rect(gfx::Point(), max_size);
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::Increment(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kIncrement;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::ShowContextMenu(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SignalEndOfTest() {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kSignalEndOfTest;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node,
    gfx::Rect subfocus,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) {
  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.ScrollToMakeVisible"));

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = subfocus;
  action_data.horizontal_scroll_alignment = horizontal_scroll_alignment;
  action_data.vertical_scroll_alignment = vertical_scroll_alignment;
  action_data.scroll_behavior = scroll_behavior;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node,
    gfx::Point point) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = point;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetScrollOffset(
    const BrowserAccessibility& node,
    gfx::Point offset) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point = offset;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetValue(const BrowserAccessibility& node,
                                           const std::string& value) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kSetValue;
  action_data.value = value;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetSelection(
    const ui::AXActionData& action_data) {
  if (!delegate_)
    return;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetSelection(
    const BrowserAccessibilityRange& range) {
  if (!delegate_ || range.IsNull())
    return;

  ui::AXActionData action_data;
  action_data.anchor_node_id = range.anchor()->anchor_id();
  action_data.anchor_offset = range.anchor()->text_offset();
  action_data.focus_node_id = range.focus()->anchor_id();
  action_data.focus_offset = range.focus()->text_offset();
  action_data.action = ax::mojom::Action::kSetSelection;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::LoadInlineTextBoxes(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kLoadInlineTextBoxes;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetAccessibilityFocus(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetAccessibilityFocus;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::ClearAccessibilityFocus(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kClearAccessibilityFocus;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::HitTest(const gfx::Point& frame_point) const {
  if (!delegate_)
    return;

  delegate_->AccessibilityHitTest(frame_point, ax::mojom::Event::kHover, 0, {});
}

gfx::Rect BrowserAccessibilityManager::GetViewBoundsInScreenCoordinates()
    const {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate)
    return delegate->AccessibilityGetViewBounds();
  return gfx::Rect();
}

// static
// Next object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::NextInTreeOrder(
    const BrowserAccessibility* object) {
  if (!object)
    return nullptr;

  if (object->PlatformChildCount())
    return object->PlatformGetFirstChild();

  while (object) {
    BrowserAccessibility* sibling = object->PlatformGetNextSibling();
    if (sibling)
      return sibling;

    object = object->PlatformGetParent();
  }

  return nullptr;
}

// static
// Next non-descendant object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::NextNonDescendantInTreeOrder(
    const BrowserAccessibility* object) {
  if (!object)
    return nullptr;

  while (object) {
    BrowserAccessibility* sibling = object->PlatformGetNextSibling();
    if (sibling)
      return sibling;

    object = object->PlatformGetParent();
  }

  return nullptr;
}

// static
// Previous object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::PreviousInTreeOrder(
    const BrowserAccessibility* object,
    bool can_wrap_to_last_element) {
  if (!object)
    return nullptr;

  // For android, this needs to be handled carefully. If not, there is a chance
  // of getting into infinite loop.
  if (can_wrap_to_last_element && object->manager()->GetRoot() == object &&
      object->PlatformChildCount() != 0) {
    return object->PlatformDeepestLastChild();
  }

  BrowserAccessibility* sibling = object->PlatformGetPreviousSibling();
  if (!sibling)
    return object->PlatformGetParent();

  if (sibling->PlatformChildCount())
    return sibling->PlatformDeepestLastChild();

  return sibling;
}

// static
BrowserAccessibility* BrowserAccessibilityManager::PreviousTextOnlyObject(
    const BrowserAccessibility* object) {
  BrowserAccessibility* previous_object = PreviousInTreeOrder(object, false);
  while (previous_object && !previous_object->IsText())
    previous_object = PreviousInTreeOrder(previous_object, false);

  return previous_object;
}

// static
BrowserAccessibility* BrowserAccessibilityManager::NextTextOnlyObject(
    const BrowserAccessibility* object) {
  BrowserAccessibility* next_object = NextInTreeOrder(object);
  while (next_object && !next_object->IsText())
    next_object = NextInTreeOrder(next_object);

  return next_object;
}

// static
bool BrowserAccessibilityManager::FindIndicesInCommonParent(
    const BrowserAccessibility& object1,
    const BrowserAccessibility& object2,
    BrowserAccessibility** common_parent,
    int* child_index1,
    int* child_index2) {
  DCHECK(common_parent && child_index1 && child_index2);
  auto* ancestor1 = const_cast<BrowserAccessibility*>(&object1);
  auto* ancestor2 = const_cast<BrowserAccessibility*>(&object2);
  do {
    *child_index1 = ancestor1->GetIndexInParent();
    ancestor1 = ancestor1->PlatformGetParent();
  } while (
      ancestor1 &&
      // |BrowserAccessibility::IsAncestorOf| returns true if objects are equal.
      (ancestor1 == ancestor2 || !ancestor2->IsDescendantOf(ancestor1)));

  if (!ancestor1) {
    *common_parent = nullptr;
    *child_index1 = -1;
    *child_index2 = -1;
    return false;
  }

  do {
    *child_index2 = ancestor2->GetIndexInParent();
    ancestor2 = ancestor2->PlatformGetParent();
  } while (ancestor1 != ancestor2);

  *common_parent = ancestor1;
  return true;
}

// static
ax::mojom::TreeOrder BrowserAccessibilityManager::CompareNodes(
    const BrowserAccessibility& object1,
    const BrowserAccessibility& object2) {
  if (&object1 == &object2)
    return ax::mojom::TreeOrder::kEqual;

  BrowserAccessibility* common_parent;
  int child_index1;
  int child_index2;
  if (FindIndicesInCommonParent(object1, object2, &common_parent, &child_index1,
                                &child_index2)) {
    if (child_index1 < child_index2)
      return ax::mojom::TreeOrder::kBefore;
    if (child_index1 > child_index2)
      return ax::mojom::TreeOrder::kAfter;
  }

  if (object2.IsDescendantOf(&object1))
    return ax::mojom::TreeOrder::kBefore;
  if (object1.IsDescendantOf(&object2))
    return ax::mojom::TreeOrder::kAfter;

  return ax::mojom::TreeOrder::kUndefined;
}

std::vector<const BrowserAccessibility*>
BrowserAccessibilityManager::FindTextOnlyObjectsInRange(
    const BrowserAccessibility& start_object,
    const BrowserAccessibility& end_object) {
  std::vector<const BrowserAccessibility*> text_only_objects;
  int child_index1 = -1;
  int child_index2 = -1;
  if (&start_object != &end_object) {
    BrowserAccessibility* common_parent;
    if (!FindIndicesInCommonParent(start_object, end_object, &common_parent,
                                   &child_index1, &child_index2)) {
      return text_only_objects;
    }

    DCHECK(common_parent);
    DCHECK_GE(child_index1, 0);
    DCHECK_GE(child_index2, 0);
    // If the child indices are equal, one object is a descendant of the other.
    DCHECK(child_index1 != child_index2 ||
           start_object.IsDescendantOf(&end_object) ||
           end_object.IsDescendantOf(&start_object));
  }

  const BrowserAccessibility* start_text_object = nullptr;
  const BrowserAccessibility* end_text_object = nullptr;
  if (&start_object == &end_object && start_object.IsPlainTextField()) {
    // We need to get to the shadow DOM that is inside the text control in order
    // to find the text-only objects.
    if (!start_object.InternalChildCount())
      return text_only_objects;
    start_text_object = start_object.InternalGetFirstChild();
    end_text_object = start_object.InternalGetLastChild();
  } else if (child_index1 <= child_index2 ||
             end_object.IsDescendantOf(&start_object)) {
    start_text_object = &start_object;
    end_text_object = &end_object;
  } else if (child_index1 > child_index2 ||
             start_object.IsDescendantOf(&end_object)) {
    start_text_object = &end_object;
    end_text_object = &start_object;
  }

  // Pre-order traversal might leave some text-only objects behind if we don't
  // start from the deepest children of the end object.
  if (!end_text_object->PlatformIsLeaf())
    end_text_object = end_text_object->PlatformDeepestLastChild();

  if (!start_text_object->IsText())
    start_text_object = NextTextOnlyObject(start_text_object);
  if (!end_text_object->IsText())
    end_text_object = PreviousTextOnlyObject(end_text_object);

  if (!start_text_object || !end_text_object)
    return text_only_objects;

  while (start_text_object && start_text_object != end_text_object) {
    text_only_objects.push_back(start_text_object);
    start_text_object = NextTextOnlyObject(start_text_object);
  }
  text_only_objects.push_back(end_text_object);

  return text_only_objects;
}

// static
base::string16 BrowserAccessibilityManager::GetTextForRange(
    const BrowserAccessibility& start_object,
    const BrowserAccessibility& end_object) {
  return GetTextForRange(start_object, 0, end_object,
                         end_object.GetInnerText().length());
}

// static
base::string16 BrowserAccessibilityManager::GetTextForRange(
    const BrowserAccessibility& start_object,
    int start_offset,
    const BrowserAccessibility& end_object,
    int end_offset) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);

  if (&start_object == &end_object && start_object.IsPlainTextField()) {
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    if (start_offset >=
            static_cast<int>(start_object.GetInnerText().length()) ||
        end_offset > static_cast<int>(start_object.GetInnerText().length())) {
      return base::string16();
    }

    return start_object.GetInnerText().substr(start_offset,
                                              end_offset - start_offset);
  }

  std::vector<const BrowserAccessibility*> text_only_objects =
      FindTextOnlyObjectsInRange(start_object, end_object);
  if (text_only_objects.empty())
    return base::string16();

  if (text_only_objects.size() == 1) {
    // Be a little permissive with the start and end offsets.
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    const BrowserAccessibility* text_object = text_only_objects[0];
    if (start_offset < static_cast<int>(text_object->GetInnerText().length()) &&
        end_offset <= static_cast<int>(text_object->GetInnerText().length())) {
      return text_object->GetInnerText().substr(start_offset,
                                                end_offset - start_offset);
    }
    return text_object->GetInnerText();
  }

  base::string16 text;
  const BrowserAccessibility* start_text_object = text_only_objects[0];
  // Figure out if the start and end positions have been reversed.
  const BrowserAccessibility* first_object = &start_object;
  if (!first_object->IsText())
    first_object = NextTextOnlyObject(first_object);
  if (!first_object || first_object != start_text_object)
    std::swap(start_offset, end_offset);

  if (start_offset <
      static_cast<int>(start_text_object->GetInnerText().length())) {
    text += start_text_object->GetInnerText().substr(start_offset);
  } else {
    text += start_text_object->GetInnerText();
  }

  for (size_t i = 1; i < text_only_objects.size() - 1; ++i) {
    text += text_only_objects[i]->GetInnerText();
  }

  const BrowserAccessibility* end_text_object = text_only_objects.back();
  if (end_offset <=
      static_cast<int>(end_text_object->GetInnerText().length())) {
    text += end_text_object->GetInnerText().substr(0, end_offset);
  } else {
    text += end_text_object->GetInnerText();
  }

  return text;
}

// static
gfx::Rect BrowserAccessibilityManager::GetRootFrameInnerTextRangeBoundsRect(
    const BrowserAccessibility& start_object,
    int start_offset,
    const BrowserAccessibility& end_object,
    int end_offset) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);

  if (&start_object == &end_object && start_object.IsPlainTextField()) {
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    if (start_offset >=
            static_cast<int>(start_object.GetInnerText().length()) ||
        end_offset > static_cast<int>(start_object.GetInnerText().length())) {
      return gfx::Rect();
    }

    return start_object.GetUnclippedRootFrameInnerTextRangeBoundsRect(
        start_offset, end_offset);
  }

  gfx::Rect result;
  const BrowserAccessibility* first = &start_object;
  const BrowserAccessibility* last = &end_object;

  switch (CompareNodes(*first, *last)) {
    case ax::mojom::TreeOrder::kBefore:
    case ax::mojom::TreeOrder::kEqual:
      break;
    case ax::mojom::TreeOrder::kAfter:
      std::swap(first, last);
      std::swap(start_offset, end_offset);
      break;
    default:
      return gfx::Rect();
  }

  const BrowserAccessibility* current = first;
  do {
    if (current->IsText()) {
      int len = static_cast<int>(current->GetInnerText().size());
      int start_char_index = 0;
      int end_char_index = len;
      if (current == first)
        start_char_index = start_offset;
      if (current == last)
        end_char_index = end_offset;
      result.Union(current->GetUnclippedRootFrameInnerTextRangeBoundsRect(
          start_char_index, end_char_index));
    } else {
      result.Union(current->GetClippedRootFrameBoundsRect());
    }

    if (current == last)
      break;

    current = NextInTreeOrder(current);
  } while (current);

  return result;
}

void BrowserAccessibilityManager::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                      ui::AXNode* node) {
  DCHECK(node);
  if (BrowserAccessibility* wrapper = GetFromAXNode(node)) {
    if (wrapper == GetLastFocusedNode())
      SetLastFocusedNode(nullptr);
  }
}

void BrowserAccessibilityManager::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {}

void BrowserAccessibilityManager::OnNodeCreated(ui::AXTree* tree,
                                                ui::AXNode* node) {
  DCHECK(node);
  BrowserAccessibility* wrapper = BrowserAccessibility::Create();
  id_wrapper_map_[node->id()] = wrapper;
  wrapper->Init(this, node);

  if (tree->root() != node &&
      node->data().role == ax::mojom::Role::kRootWebArea) {
    popup_root_ids_.insert(node->id());
  }
}

void BrowserAccessibilityManager::OnNodeDeleted(ui::AXTree* tree,
                                                int32_t node_id) {
  DCHECK_NE(node_id, ui::AXNode::kInvalidAXID);
  if (BrowserAccessibility* wrapper = GetFromID(node_id)) {
    id_wrapper_map_.erase(node_id);
    delete wrapper;
  }

  if (popup_root_ids_.find(node_id) != popup_root_ids_.end())
    popup_root_ids_.erase(node_id);
}

void BrowserAccessibilityManager::OnNodeReparented(ui::AXTree* tree,
                                                   ui::AXNode* node) {
  DCHECK(node);
  BrowserAccessibility* wrapper = GetFromAXNode(node);
  if (!wrapper) {
    wrapper = BrowserAccessibility::Create();
    id_wrapper_map_[node->id()] = wrapper;
  }
  wrapper->Init(this, node);
}

void BrowserAccessibilityManager::OnRoleChanged(ui::AXTree* tree,
                                                ui::AXNode* node,
                                                ax::mojom::Role old_role,
                                                ax::mojom::Role new_role) {
  DCHECK(node);
  if (tree->root() == node)
    return;
  if (new_role == ax::mojom::Role::kRootWebArea) {
    popup_root_ids_.insert(node->id());
  } else if (old_role == ax::mojom::Role::kRootWebArea) {
    popup_root_ids_.erase(node->id());
  }
}

void BrowserAccessibilityManager::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  const bool ax_tree_id_changed =
      GetTreeData().tree_id != ui::AXTreeIDUnknown() &&
      GetTreeData().tree_id != ax_tree_id_;
  // When the tree that contains the focus is destroyed and re-created, we
  // should fire a new focus event. Also, whenever the tree ID or the root of
  // this tree changes we may need to fire an event on our parent node in the
  // parent tree to ensure that we're properly connected.
  if (ax_tree_id_changed && last_focused_node_tree_id_ &&
      ax_tree_id_ == *last_focused_node_tree_id_) {
    SetLastFocusedNode(nullptr);
  }
  if (ax_tree_id_changed || root_changed)
    connected_to_parent_tree_node_ = false;

  if (ax_tree_id_changed) {
    ui::AXTreeManagerMap::GetInstance().RemoveTreeManager(ax_tree_id_);
    ax_tree_id_ = GetTreeData().tree_id;
    ui::AXTreeManagerMap::GetInstance().AddTreeManager(ax_tree_id_, this);
  }

  // Calls OnDataChanged on newly created, reparented or changed nodes.
  for (const auto& change : changes) {
    ui::AXNode* node = change.node;
    BrowserAccessibility* wrapper = GetFromAXNode(node);
    if (wrapper) {
      wrapper->OnDataChanged();
    }
  }
}

ui::AXNode* BrowserAccessibilityManager::GetNodeFromTree(
    const ui::AXTreeID tree_id,
    const ui::AXNode::AXID node_id) const {
  auto* manager = BrowserAccessibilityManager::FromID(tree_id);
  return manager ? manager->GetNodeFromTree(node_id) : nullptr;
}

ui::AXNode* BrowserAccessibilityManager::GetNodeFromTree(
    const ui::AXNode::AXID node_id) const {
  BrowserAccessibility* wrapper = GetFromID(node_id);
  return wrapper ? wrapper->node() : nullptr;
}

AXTreeID BrowserAccessibilityManager::GetTreeID() const {
  return ax_tree_id();
}

AXTreeID BrowserAccessibilityManager::GetParentTreeID() const {
  return GetTreeData().parent_tree_id;
}

ui::AXNode* BrowserAccessibilityManager::GetRootAsAXNode() const {
  // tree_ is nullptr after destruction.
  if (!ax_tree())
    return nullptr;

  // tree_->root() can be null during AXTreeObserver callbacks.
  return ax_tree()->root();
}

ui::AXNode* BrowserAccessibilityManager::GetParentNodeFromParentTreeAsAXNode()
    const {
  if (!GetRootAsAXNode())
    return nullptr;

  ui::AXTreeID parent_tree_id = GetParentTreeID();
  BrowserAccessibilityManager* parent_manager =
      BrowserAccessibilityManager::FromID(parent_tree_id);
  if (!parent_manager)
    return nullptr;

  std::set<int32_t> host_node_ids =
      parent_manager->ax_tree()->GetNodeIdsForChildTreeId(ax_tree_id_);

#if !defined(NDEBUG)
  if (host_node_ids.size() > 1)
    DLOG(WARNING) << "Multiple nodes claim the same child tree id.";
#endif

  for (int32_t host_node_id : host_node_ids) {
    ui::AXNode* parent_node =
        parent_manager->GetNodeFromTree(parent_tree_id, host_node_id);
    if (parent_node) {
      DCHECK_EQ(ax_tree_id_,
                AXTreeID::FromString(parent_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kChildTreeId)));
      return parent_node;
    }
  }

  return nullptr;
}

BrowserAccessibilityManager* BrowserAccessibilityManager::GetRootManager()
    const {
  BrowserAccessibility* parent = GetParentNodeFromParentTree();
  if (parent)
    return parent->manager() ? parent->manager()->GetRootManager() : nullptr;

  if (IsRootTree())
    return const_cast<BrowserAccessibilityManager*>(this);

  // The current tree is disconnected from its parent, so we can't retrieve the
  // root manager yet.
  return nullptr;
}

BrowserAccessibilityDelegate*
BrowserAccessibilityManager::GetDelegateFromRootManager() const {
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager)
    return root_manager->delegate();
  return nullptr;
}

bool BrowserAccessibilityManager::IsRootTree() const {
  return delegate_ && delegate_->AccessibilityIsMainFrame() &&
         GetTreeData().parent_tree_id == ui::AXTreeIDUnknown();
}

// static
void BrowserAccessibilityManager::SetLastFocusedNode(
    BrowserAccessibility* node) {
  if (node) {
    DCHECK(node->manager());
    last_focused_node_id_ = node->GetId();
    last_focused_node_tree_id_ = node->manager()->ax_tree_id();
  } else {
    last_focused_node_id_.reset();
    last_focused_node_tree_id_.reset();
  }
}

// static
BrowserAccessibility* BrowserAccessibilityManager::GetLastFocusedNode() {
  if (last_focused_node_id_) {
    DCHECK(last_focused_node_tree_id_);
    if (BrowserAccessibilityManager* last_focused_manager =
            FromID(last_focused_node_tree_id_.value()))
      return last_focused_manager->GetFromID(last_focused_node_id_.value());
  }
  return nullptr;
}

ui::AXTreeUpdate BrowserAccessibilityManager::SnapshotAXTreeForTesting() {
  std::unique_ptr<
      ui::AXTreeSource<const ui::AXNode*, ui::AXNodeData, ui::AXTreeData>>
      tree_source(tree_->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*, ui::AXNodeData, ui::AXTreeData>
      serializer(tree_source.get());
  ui::AXTreeUpdate update;
  serializer.SerializeChanges(GetRootAsAXNode(), &update);
  return update;
}

void BrowserAccessibilityManager::UseCustomDeviceScaleFactorForTesting(
    float device_scale_factor) {
  use_custom_device_scale_factor_for_testing_ = true;
  device_scale_factor_ = device_scale_factor;
}

BrowserAccessibility* BrowserAccessibilityManager::CachingAsyncHitTest(
    const gfx::Point& physical_pixel_point) const {
  // TODO(crbug.com/1061323): By starting the hit test on the root frame,
  // it allows for the possibility that we don't return a descendant as the
  // hit test result, but AXPlatformNodeDelegate says that it's only supposed
  // to return a descendant, so this isn't correctly fulfilling the contract.
  // Unchecked it can even lead to an infinite loop.
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager && root_manager != this)
    return root_manager->CachingAsyncHitTest(physical_pixel_point);

  gfx::Point blink_screen_point =
      IsUseZoomForDSFEnabled()
          ? physical_pixel_point
          : ScaleToRoundedPoint(physical_pixel_point,
                                1.0 / device_scale_factor());

  gfx::Rect screen_view_bounds = GetViewBoundsInScreenCoordinates();

  if (delegate_) {
    // Transform from screen to viewport to frame coordinates to pass to Blink.
    // Note that page scale (pinch zoom) is independent of device scale factor
    // (display DPI). Only the latter is affected by UseZoomForDSF.
    // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
    gfx::Point viewport_point =
        blink_screen_point - screen_view_bounds.OffsetFromOrigin();
    gfx::Point frame_point =
        gfx::ScaleToRoundedPoint(viewport_point, 1.0f / page_scale_factor_);

    // This triggers an asynchronous request to compute the true object that's
    // under the point.
    HitTest(frame_point);

    // Unfortunately we still have to return an answer synchronously because
    // the APIs were designed that way. The best case scenario is that the
    // screen point is within the bounds of the last result we got from a
    // call to AccessibilityHitTest - in that case, we can return that object!
    if (last_hover_bounds_.Contains(blink_screen_point)) {
      BrowserAccessibilityManager* manager =
          BrowserAccessibilityManager::FromID(last_hover_ax_tree_id_);
      if (manager) {
        BrowserAccessibility* node = manager->GetFromID(last_hover_node_id_);
        if (node)
          return node;
      }
    }
  }

  // If that test failed we have to fall back on searching the accessibility
  // tree locally for the best bounding box match. This is generally right
  // for simple pages but wrong in cases of z-index, overflow, and other
  // more complicated layouts. The hope is that if the user is moving the
  // mouse, this fallback will only be used transiently, and the asynchronous
  // result will be used for the next call.
  return GetRoot()->ApproximateHitTest(blink_screen_point);
}

void BrowserAccessibilityManager::CacheHitTestResult(
    BrowserAccessibility* hit_test_result) const {
  // Walk up to the highest ancestor that's a leaf node; we don't want to
  // return a node that's hidden from the tree.
  hit_test_result = hit_test_result->PlatformGetClosestPlatformObject();

  last_hover_ax_tree_id_ = hit_test_result->manager()->ax_tree_id();
  last_hover_node_id_ = hit_test_result->GetId();
  last_hover_bounds_ = hit_test_result->GetClippedScreenBoundsRect();
}

void BrowserAccessibilityManager::DidActivatePortal(
    WebContents* predecessor_contents,
    base::TimeTicks activation_time) {
  if (GetTreeData().loaded) {
    FireGeneratedEvent(ui::AXEventGenerator::Event::PORTAL_ACTIVATED,
                       GetRoot());
  }
}

void BrowserAccessibilityManager::SetPageScaleFactor(float page_scale_factor) {
  page_scale_factor_ = page_scale_factor;
}

float BrowserAccessibilityManager::GetPageScaleFactor() const {
  return page_scale_factor_;
}

void BrowserAccessibilityManager::CollectChangedNodesAndParentsForAtomicUpdate(
    ui::AXTree* tree,
    const std::vector<ui::AXTreeObserver::Change>& changes,
    std::set<ui::AXPlatformNode*>* nodes_needing_update) {
  // The nodes that need to be updated are all of the nodes that were changed,
  // plus some parents.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);

    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj)
      nodes_needing_update->insert(obj->GetAXPlatformNode());

    // When a node is a text node or line break, update its parent, because
    // its text is part of its hypertext.
    const ui::AXNode* parent = changed_node->GetUnignoredParent();
    if (!parent)
      continue;

    if (changed_node->IsText() &&
        changed_node->data().role != ax::mojom::Role::kInlineTextBox) {
      BrowserAccessibility* parent_obj = GetFromAXNode(parent);
      if (parent_obj)
        nodes_needing_update->insert(parent_obj->GetAXPlatformNode());
    }

    // When a node is editable, update the editable root too.
    if (!changed_node->data().HasState(ax::mojom::State::kEditable))
      continue;
    const ui::AXNode* editable_root = changed_node;
    while (editable_root->parent() && editable_root->parent()->data().HasState(
                                          ax::mojom::State::kEditable)) {
      editable_root = editable_root->parent();
    }

    BrowserAccessibility* editable_root_obj = GetFromAXNode(editable_root);
    if (editable_root_obj)
      nodes_needing_update->insert(editable_root_obj->GetAXPlatformNode());
  }
}

bool BrowserAccessibilityManager::ShouldFireEventForNode(
    BrowserAccessibility* node) const {
  node = RetargetForEvents(node, RetargetEventType::RetargetEventTypeGenerated);
  if (!node || !node->CanFireEvents())
    return false;

  // If the root delegate isn't the main-frame, this may be a new frame that
  // hasn't yet been swapped in or added to the frame tree. Suppress firing
  // events until then.
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  if (!root_delegate->AccessibilityIsMainFrame())
    return false;

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return false;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to the platform.
  if (node->GetRole() == ax::mojom::Role::kInlineTextBox)
    return false;

  return true;
}

}  // namespace content
