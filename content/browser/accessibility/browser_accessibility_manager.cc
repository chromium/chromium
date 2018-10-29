// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include <stddef.h>
#include <map>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace content {

namespace {

// Map from AXTreeID to BrowserAccessibilityManager
using AXTreeIDMap = std::map<ui::AXTreeID, BrowserAccessibilityManager*>;
base::LazyInstance<AXTreeIDMap>::Leaky g_ax_tree_id_map =
    LAZY_INSTANCE_INITIALIZER;

// A function to call when focus changes, for testing only.
base::LazyInstance<base::Closure>::DestructorAtExit
    g_focus_change_callback_for_testing = LAZY_INSTANCE_INITIALIZER;

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
  tree_data.tree_id = ui::AXTreeID::FromString("1");
  tree_data.focused_tree_id = ui::AXTreeID::FromString("1");
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

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
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
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(initial_tree, delegate, factory);
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::FromID(
    ui::AXTreeID ax_tree_id) {
  AXTreeIDMap* ax_tree_id_map = g_ax_tree_id_map.Pointer();
  auto iter = ax_tree_id_map->find(ax_tree_id);
  return iter == ax_tree_id_map->end() ? nullptr : iter->second;
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : AXEventGenerator(),
      delegate_(delegate),
      factory_(factory),
      tree_(new ui::AXSerializableTree()),
      user_is_navigating_away_(false),
      last_focused_node_(nullptr),
      last_focused_manager_(nullptr),
      connected_to_parent_tree_node_(false),
      ax_tree_id_(ui::AXTreeIDUnknown()),
      device_scale_factor_(1.0f),
      use_custom_device_scale_factor_for_testing_(false) {
  SetTree(tree_.get());
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : AXEventGenerator(),
      delegate_(delegate),
      factory_(factory),
      tree_(new ui::AXSerializableTree()),
      user_is_navigating_away_(false),
      last_focused_node_(nullptr),
      last_focused_manager_(nullptr),
      ax_tree_id_(ui::AXTreeIDUnknown()),
      device_scale_factor_(1.0f),
      use_custom_device_scale_factor_for_testing_(false) {
  SetTree(tree_.get());
  Initialize(initial_tree);
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  tree_.reset(nullptr);
  ReleaseTree();
  g_ax_tree_id_map.Get().erase(ax_tree_id_);
}

void BrowserAccessibilityManager::Initialize(
    const ui::AXTreeUpdate& initial_tree) {
  if (!tree_->Unserialize(initial_tree)) {
    static auto* ax_tree_error = base::debug::AllocateCrashKeyString(
        "ax_tree_error", base::debug::CrashKeySize::Size32);
    static auto* ax_tree_update = base::debug::AllocateCrashKeyString(
        "ax_tree_update", base::debug::CrashKeySize::Size64);
    // Temporarily log some additional crash keys so we can try to
    // figure out why we're getting bad accessibility trees here.
    // http://crbug.com/765490
    // Be sure to re-enable BrowserAccessibilityManagerTest.TestFatalError
    // when done (or delete it if no longer needed).
    base::debug::SetCrashKeyString(ax_tree_error, tree_->error());
    base::debug::SetCrashKeyString(ax_tree_update, initial_tree.ToString());
    LOG(FATAL) << tree_->error();
  }
}

// A flag for use in tests to ensure events aren't suppressed or delayed.
// static
bool BrowserAccessibilityManager::never_suppress_or_delay_events_for_testing_ =
    false;

// static
ui::AXTreeUpdate
BrowserAccessibilityManager::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManager::FireFocusEventsIfNeeded() {
  BrowserAccessibility* focus = GetFocus();

  // Don't fire focus events if the window itself doesn't have focus.
  // Bypass this check for some tests.
  if (!never_suppress_or_delay_events_for_testing_ &&
      !g_focus_change_callback_for_testing.Get()) {
    if (delegate_ && !delegate_->AccessibilityViewHasFocus())
      focus = nullptr;

    if (!CanFireEvents())
      focus = nullptr;
  }

  // Don't allow the document to be focused if it has no children and
  // hasn't finished loading yet. Wait for at least a tiny bit of content,
  // or for the document to actually finish loading.
  if (focus && focus == focus->manager()->GetRoot() &&
      focus->PlatformChildCount() == 0 &&
      !focus->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy) &&
      !focus->manager()->GetTreeData().loaded) {
    focus = nullptr;
  }

  if (focus && focus != last_focused_node_)
    FireFocusEvent(focus);

  last_focused_node_ = focus;
  last_focused_manager_ = focus ? focus->manager() : nullptr;
}

bool BrowserAccessibilityManager::CanFireEvents() {
  return true;
}

void BrowserAccessibilityManager::FireFocusEvent(BrowserAccessibility* node) {
  if (g_focus_change_callback_for_testing.Get())
    g_focus_change_callback_for_testing.Get().Run();
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  // tree_ can be null during destruction.
  if (!tree_)
    return nullptr;

  // tree_->root() can be null during AXTreeDelegate callbacks.
  ui::AXNode* root = tree_->root();
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

BrowserAccessibility*
BrowserAccessibilityManager::GetParentNodeFromParentTree() {
  if (!GetRoot())
    return nullptr;

  ui::AXTreeID parent_tree_id = GetTreeData().parent_tree_id;
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
    BrowserAccessibility* parent_node = parent_manager->GetFromID(host_node_id);
    if (parent_node) {
      DCHECK_EQ(ax_tree_id_,
                AXTreeID::FromString(parent_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kChildTreeId)));
      return parent_node;
    }
  }

  return nullptr;
}

const ui::AXTreeData& BrowserAccessibilityManager::GetTreeData() {
  return tree_->data();
}

void BrowserAccessibilityManager::OnWindowFocused() {
  if (this == GetRootManager())
    FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::OnWindowBlurred() {
  if (this == GetRootManager()) {
    last_focused_node_ = nullptr;
    last_focused_manager_ = nullptr;
  }
}

void BrowserAccessibilityManager::UserIsNavigatingAway() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::UserIsReloading() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::NavigationSucceeded() {
  user_is_navigating_away_ = false;
}

void BrowserAccessibilityManager::NavigationFailed() {
  user_is_navigating_away_ = false;
}

void BrowserAccessibilityManager::DidStopLoading() {
  user_is_navigating_away_ = false;
}

bool BrowserAccessibilityManager::UseRootScrollOffsetsWhenComputingBounds() {
  return true;
}

void BrowserAccessibilityManager::OnAccessibilityEvents(
    const AXEventNotificationDetails& details) {
  TRACE_EVENT0("accessibility",
               "BrowserAccessibilityManager::OnAccessibilityEvents");

  // Update the cached device scale factor.
  if (delegate_ && !use_custom_device_scale_factor_for_testing_)
    device_scale_factor_ = delegate_->AccessibilityGetDeviceScaleFactor();

  // Process all changes to the accessibility tree first.
  for (uint32_t index = 0; index < details.updates.size(); ++index) {
    if (!tree_->Unserialize(details.updates[index])) {
      if (delegate_) {
        LOG(ERROR) << tree_->error();
        delegate_->AccessibilityFatalError();
      } else {
        CHECK(false) << tree_->error();
      }
      return;
    }
  }

  // If this page is hidden by an interstitial, suppress all events.
  if (GetRootManager()->hidden_by_interstitial_page()) {
    ClearEvents();
    return;
  }

  // If the root's parent is in another accessibility tree but it wasn't
  // previously connected, post the proper notifications on the parent.
  BrowserAccessibility* parent = GetParentNodeFromParentTree();
  if (parent) {
    if (!connected_to_parent_tree_node_) {
      parent->OnDataChanged();
      parent->UpdatePlatformAttributes();
      FireGeneratedEvent(Event::CHILDREN_CHANGED, parent);
      connected_to_parent_tree_node_ = true;
    }
  } else {
    connected_to_parent_tree_node_ = false;
  }

  // Based on the changes to the tree, fire focus events if needed.
  // Screen readers might not do the right thing if they're not aware of what
  // has focus, so always try that first. Nothing will be fired if the window
  // itself isn't focused or if focus hasn't changed.
  GetRootManager()->FireFocusEventsIfNeeded();

  // Fire any events related to changes to the tree.
  for (auto targeted_event : *this) {
    BrowserAccessibility* event_target = GetFromAXNode(targeted_event.node);
    if (!event_target || event_target->PlatformIsChildOfLeaf())
      continue;

    FireGeneratedEvent(targeted_event.event_params.event, event_target);
  }
  ClearEvents();

  // Fire events from Blink.
  for (uint32_t index = 0; index < details.events.size(); index++) {
    const ui::AXEvent& event = details.events[index];

    // Fire the native event.
    BrowserAccessibility* event_target = GetFromID(event.id);
    if (!event_target || event_target->PlatformIsChildOfLeaf())
      return;

    if (event.event_type == ax::mojom::Event::kHover)
      GetRootManager()->CacheHitTestResult(event_target);

    FireBlinkEvent(event.event_type, event_target);
  }
}

void BrowserAccessibilityManager::OnLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  for (size_t i = 0; i < params.size(); ++i) {
    BrowserAccessibility* obj = GetFromID(params[i].id);
    if (!obj)
      continue;
    ui::AXNode* node = obj->node();
    node->SetLocation(params[i].new_location.offset_container_id,
                      params[i].new_location.bounds,
                      params[i].new_location.transform.get());
  }
  SendLocationChangeEvents(params);
}

void BrowserAccessibilityManager::SendLocationChangeEvents(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  for (size_t i = 0; i < params.size(); ++i) {
    BrowserAccessibility* obj = GetFromID(params[i].id);
    if (obj)
      obj->OnLocationChanged();
  }
}

void BrowserAccessibilityManager::OnFindInPageResult(
    int request_id, int match_index, int start_id, int start_offset,
    int end_id, int end_offset) {
  find_in_page_info_.request_id = request_id;
  find_in_page_info_.match_index = match_index;
  find_in_page_info_.start_id = start_id;
  find_in_page_info_.start_offset = start_offset;
  find_in_page_info_.end_id = end_id;
  find_in_page_info_.end_offset = end_offset;

  if (find_in_page_info_.active_request_id == request_id)
    ActivateFindInPageResult(request_id);
}

void BrowserAccessibilityManager::ActivateFindInPageResult(
    int request_id) {
  find_in_page_info_.active_request_id = request_id;
  if (find_in_page_info_.request_id != request_id)
    return;

  BrowserAccessibility* node = GetFromID(find_in_page_info_.start_id);
  if (!node)
    return;

  // If an ancestor of this node is a leaf node, fire the notification on that.
  node = node->GetClosestPlatformObject();

  // The "scrolled to anchor" notification is a great way to get a
  // screen reader to jump directly to a specific location in a document.
  FireBlinkEvent(ax::mojom::Event::kScrolledToAnchor, node);
}

BrowserAccessibility* BrowserAccessibilityManager::GetActiveDescendant(
    BrowserAccessibility* focus) {
  if (!focus)
    return nullptr;

  int32_t active_descendant_id;
  BrowserAccessibility* active_descendant = nullptr;
  if (focus->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                             &active_descendant_id)) {
    active_descendant = focus->manager()->GetFromID(active_descendant_id);
  }

  if (focus->GetRole() == ax::mojom::Role::kPopUpButton) {
    BrowserAccessibility* child = focus->InternalGetChild(0);
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

BrowserAccessibility* BrowserAccessibilityManager::GetFocus() {
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (!root_manager)
    root_manager = this;
  ui::AXTreeID focused_tree_id = root_manager->GetTreeData().focused_tree_id;

  BrowserAccessibilityManager* focused_manager = nullptr;
  if (focused_tree_id != ui::AXTreeIDUnknown())
    focused_manager = BrowserAccessibilityManager::FromID(focused_tree_id);

  // BrowserAccessibilityManager::FromID(focused_tree_id) may return nullptr
  // if the tree is not created or has been destroyed.
  if (!focused_manager)
    focused_manager = root_manager;

  return focused_manager->GetFocusFromThisOrDescendantFrame();
}

BrowserAccessibility*
BrowserAccessibilityManager::GetFocusFromThisOrDescendantFrame() {
  int32_t focus_id = GetTreeData().focus_id;
  BrowserAccessibility* obj = GetFromID(focus_id);
  if (!obj)
    return GetRoot();

  if (obj->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        obj->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(child_tree_id);
    if (child_manager)
      return child_manager->GetFocusFromThisOrDescendantFrame();
  }

  return obj;
}

void BrowserAccessibilityManager::SetFocus(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetFocusLocallyForTesting(
    BrowserAccessibility* node) {
  ui::AXTreeData data = GetTreeData();
  data.focus_id = node->GetId();
  tree_->UpdateData(data);
}

// static
void BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
    const base::Closure& callback) {
  g_focus_change_callback_for_testing.Get() = callback;
}

// static
void BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting() {
  never_suppress_or_delay_events_for_testing_ = true;
}

void BrowserAccessibilityManager::Decrement(
    const BrowserAccessibility& node) {
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

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::GetImageData(
    const BrowserAccessibility& node,
    const gfx::Size& max_size) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kGetImageData;
  action_data.target_node_id = node.GetId();
  action_data.target_rect = gfx::Rect(gfx::Point(), max_size);
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::Increment(
    const BrowserAccessibility& node) {
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

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node, gfx::Rect subfocus) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = subfocus;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node, gfx::Point point) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = point;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::SetScrollOffset(
    const BrowserAccessibility& node, gfx::Point offset) {
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

void BrowserAccessibilityManager::SetSelection(AXPlatformRange range) {
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

void BrowserAccessibilityManager::HitTest(const gfx::Point& point) {
  if (!delegate_)
    return;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point = point;
  action_data.hit_test_event_to_fire = ax::mojom::Event::kHover;
  delegate_->AccessibilityPerformAction(action_data);
}

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
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
    return object->PlatformGetChild(0);

  while (object) {
    BrowserAccessibility* sibling = object->GetNextSibling();
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
  if (can_wrap_to_last_element &&
      object->GetRole() == ax::mojom::Role::kRootWebArea &&
      object->PlatformChildCount() != 0) {
    return object->PlatformDeepestLastChild();
  }

  BrowserAccessibility* sibling = object->GetPreviousSibling();
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
  while (previous_object && !previous_object->IsTextOnlyObject())
    previous_object = PreviousInTreeOrder(previous_object, false);

  return previous_object;
}

// static
BrowserAccessibility* BrowserAccessibilityManager::NextTextOnlyObject(
    const BrowserAccessibility* object) {
  BrowserAccessibility* next_object = NextInTreeOrder(object);
  while (next_object && !next_object->IsTextOnlyObject())
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
  if (FindIndicesInCommonParent(
          object1, object2, &common_parent, &child_index1, &child_index2)) {
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
    start_text_object = start_object.InternalGetChild(0);
    end_text_object =
        start_object.InternalGetChild(start_object.InternalChildCount() - 1);
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

  if (!start_text_object->IsTextOnlyObject())
    start_text_object = NextTextOnlyObject(start_text_object);
  if (!end_text_object->IsTextOnlyObject())
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
                         end_object.GetText().length());
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

    if (start_offset >= static_cast<int>(start_object.GetText().length()) ||
        end_offset > static_cast<int>(start_object.GetText().length())) {
      return base::string16();
    }

    return start_object.GetText().substr(start_offset,
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
    if (start_offset < static_cast<int>(text_object->GetText().length()) &&
        end_offset <= static_cast<int>(text_object->GetText().length())) {
      return text_object->GetText().substr(start_offset,
                                           end_offset - start_offset);
    }
    return text_object->GetText();
  }

  base::string16 text;
  const BrowserAccessibility* start_text_object = text_only_objects[0];
  // Figure out if the start and end positions have been reversed.
  const BrowserAccessibility* first_object = &start_object;
  if (!first_object->IsTextOnlyObject())
    first_object = NextTextOnlyObject(first_object);
  if (!first_object || first_object != start_text_object)
    std::swap(start_offset, end_offset);

  if (start_offset < static_cast<int>(start_text_object->GetText().length())) {
    text += start_text_object->GetText().substr(start_offset);
  } else {
    text += start_text_object->GetText();
  }

  for (size_t i = 1; i < text_only_objects.size() - 1; ++i) {
    text += text_only_objects[i]->GetText();
  }

  const BrowserAccessibility* end_text_object = text_only_objects.back();
  if (end_offset <= static_cast<int>(end_text_object->GetText().length())) {
    text += end_text_object->GetText().substr(0, end_offset);
  } else {
    text += end_text_object->GetText();
  }

  return text;
}

// static
gfx::Rect BrowserAccessibilityManager::GetPageBoundsForRange(
    const BrowserAccessibility& start_object,
    int start_offset,
    const BrowserAccessibility& end_object,
    int end_offset) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);

  if (&start_object == &end_object && start_object.IsPlainTextField()) {
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    if (start_offset >= static_cast<int>(start_object.GetText().length()) ||
        end_offset > static_cast<int>(start_object.GetText().length())) {
      return gfx::Rect();
    }

    return start_object.GetPageBoundsForRange(
        start_offset, end_offset - start_offset);
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
    if (current->IsTextOnlyObject()) {
      int len = static_cast<int>(current->GetText().size());
      int start_char_index = 0;
      int end_char_index = len;
      if (current == first)
        start_char_index = start_offset;
      if (current == last)
        end_char_index = end_offset;
      result.Union(current->GetPageBoundsForRange(
          start_char_index, end_char_index - start_char_index));
    } else {
      result.Union(current->GetPageBoundsRect());
    }

    if (current == last)
      break;

    current = NextInTreeOrder(current);
  } while (current);

  return result;
}

void BrowserAccessibilityManager::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                      ui::AXNode* node) {
  AXEventGenerator::OnNodeWillBeDeleted(tree, node);
  DCHECK(node);
  if (id_wrapper_map_.find(node->id()) == id_wrapper_map_.end())
    return;
  GetFromAXNode(node)->Destroy();
  id_wrapper_map_.erase(node->id());
}

void BrowserAccessibilityManager::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {
  AXEventGenerator::OnSubtreeWillBeDeleted(tree, node);
  DCHECK(node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj)
    obj->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManager::OnNodeCreated(ui::AXTree* tree,
                                                ui::AXNode* node) {
  AXEventGenerator::OnNodeCreated(tree, node);
  BrowserAccessibility* wrapper = factory_->Create();
  wrapper->Init(this, node);
  id_wrapper_map_[node->id()] = wrapper;
  wrapper->OnDataChanged();
}

void BrowserAccessibilityManager::OnNodeReparented(ui::AXTree* tree,
                                                   ui::AXNode* node) {
  AXEventGenerator::OnNodeReparented(tree, node);
  BrowserAccessibility* wrapper = GetFromID(node->id());
  if (!wrapper) {
    wrapper = factory_->Create();
    id_wrapper_map_[node->id()] = wrapper;
  }

  wrapper->Init(this, node);
  wrapper->OnDataChanged();
}

void BrowserAccessibilityManager::OnNodeChanged(ui::AXTree* tree,
                                                ui::AXNode* node) {
  AXEventGenerator::OnNodeChanged(tree, node);
  DCHECK(node);
  GetFromAXNode(node)->OnDataChanged();
}

void BrowserAccessibilityManager::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  AXEventGenerator::OnAtomicUpdateFinished(tree, root_changed, changes);
  bool ax_tree_id_changed = false;
  if (GetTreeData().tree_id != ui::AXTreeIDUnknown() &&
      GetTreeData().tree_id != ax_tree_id_) {
    g_ax_tree_id_map.Get().erase(ax_tree_id_);
    ax_tree_id_ = GetTreeData().tree_id;
    g_ax_tree_id_map.Get().insert(std::make_pair(ax_tree_id_, this));
    ax_tree_id_changed = true;
  }

  // Whenever the tree ID or the root of this tree changes we may need to
  // fire an event on our parent node in the parent tree to ensure that
  // we're properly connected.
  if (ax_tree_id_changed || root_changed)
    connected_to_parent_tree_node_ = false;

  // When the root changes and this is the root manager, we may need to
  // fire a new focus event.
  if (root_changed && last_focused_manager_ == this) {
    last_focused_node_ = nullptr;
    last_focused_manager_ = nullptr;
  }
}

BrowserAccessibilityManager* BrowserAccessibilityManager::GetRootManager() {
  BrowserAccessibility* parent = GetParentNodeFromParentTree();
  if (!parent)
    return this;

  return parent->manager()->GetRootManager();
}

BrowserAccessibilityDelegate*
    BrowserAccessibilityManager::GetDelegateFromRootManager() {
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager)
    return root_manager->delegate();
  return nullptr;
}

bool BrowserAccessibilityManager::IsRootTree() {
  return delegate() && delegate()->AccessibilityGetAcceleratedWidget();
}

ui::AXTreeUpdate
BrowserAccessibilityManager::SnapshotAXTreeForTesting() {
  std::unique_ptr<
      ui::AXTreeSource<const ui::AXNode*, ui::AXNodeData, ui::AXTreeData>>
      tree_source(tree_->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*,
                       ui::AXNodeData,
                       ui::AXTreeData> serializer(tree_source.get());
  ui::AXTreeUpdate update;
  serializer.SerializeChanges(tree_->root(), &update);
  return update;
}

void BrowserAccessibilityManager::UseCustomDeviceScaleFactorForTesting(
    float device_scale_factor) {
  use_custom_device_scale_factor_for_testing_ = true;
  device_scale_factor_ = device_scale_factor;
}

BrowserAccessibility* BrowserAccessibilityManager::CachingAsyncHitTest(
    const gfx::Point& screen_point) {
  gfx::Point scaled_point =
      IsUseZoomForDSFEnabled()
          ? ScaleToRoundedPoint(screen_point, device_scale_factor())
          : screen_point;

  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager && root_manager != this)
    return root_manager->CachingAsyncHitTest(scaled_point);

  if (delegate()) {
    // This triggers an asynchronous request to compute the true object that's
    // under |scaled_point|.
    HitTest(scaled_point - GetViewBounds().OffsetFromOrigin());

    // Unfortunately we still have to return an answer synchronously because
    // the APIs were designed that way. The best case scenario is that the
    // screen point is within the bounds of the last result we got from a
    // call to AccessibilityHitTest - in that case, we can return that object!
    if (last_hover_bounds_.Contains(scaled_point)) {
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
  return GetRoot()->ApproximateHitTest(screen_point);
}

void BrowserAccessibilityManager::CacheHitTestResult(
    BrowserAccessibility* hit_test_result) {
  // Walk up to the highest ancestor that's a leaf node; we don't want to
  // return a node that's hidden from the tree.
  BrowserAccessibility* parent = hit_test_result->PlatformGetParent();
  while (parent) {
    if (parent->PlatformChildCount() == 0)
      hit_test_result = parent;
    parent = parent->PlatformGetParent();
  }

  last_hover_ax_tree_id_ = hit_test_result->manager()->ax_tree_id();
  last_hover_node_id_ = hit_test_result->GetId();
  last_hover_bounds_ = hit_test_result->GetClippedScreenBoundsRect();
}

}  // namespace content
