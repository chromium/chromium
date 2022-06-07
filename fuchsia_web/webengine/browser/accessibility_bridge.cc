// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/accessibility_bridge.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include <algorithm>

#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_widget_host_view.h"
#include "fuchsia_web/webengine/browser/frame_window_tree_host.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

// TODO(https://crbug.com/973095): Update this value based on average and
// maximum sizes of serialized Semantic Nodes.
constexpr size_t kMaxNodesPerUpdate = 16;

constexpr size_t kMaxNodesPerDelete =
    fuchsia::accessibility::semantics::MAX_NODES_PER_UPDATE;

// Error allowed for each edge when converting from gfx::RectF to gfx::Rect.
constexpr float kRectConversionError = 0.5;

// Inspect node/property names.
constexpr char kSemanticTreesInspectNodeName[] = "trees";
constexpr char kSemanticTreeContentsInspectPropertyName[] = "contents";
constexpr char kParentTreeInspectPropertyName[] = "parent_tree";
constexpr char kParentNodeInspectPropertyName[] = "parent_node";

// Returns the id of the offset container for |node|, or the root node id if
// |node| does not specify an offset container.
int32_t GetOffsetContainerId(const ui::AXTree* tree,
                             const ui::AXNodeData& node_data) {
  int32_t offset_container_id = node_data.relative_bounds.offset_container_id;
  if (offset_container_id == -1)
    return tree->root()->id();
  return offset_container_id;
}

}  // namespace

AccessibilityBridge::AccessibilityBridge(
    fuchsia::accessibility::semantics::SemanticsManager* semantics_manager,
    FrameWindowTreeHost* window_tree_host,
    content::WebContents* web_contents,
    base::OnceCallback<bool(zx_status_t)> on_error_callback,
    inspect::Node inspect_node)
    : binding_(this),
      window_tree_host_(window_tree_host),
      web_contents_(web_contents),
      on_error_callback_(std::move(on_error_callback)),
      inspect_node_(std::move(inspect_node)) {
  DCHECK(web_contents_);
  Observe(web_contents_);

  semantics_manager->RegisterViewForSemantics(
      window_tree_host_->CreateViewRef(), binding_.NewBinding(),
      semantic_tree_.NewRequest());
  semantic_tree_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "SemanticTree disconnected";
    std::move(on_error_callback_).Run(ZX_ERR_INTERNAL);
  });

  // Set up inspect node for semantic trees.
  inspect_node_tree_dump_ = inspect_node_.CreateLazyNode(
      kSemanticTreesInspectNodeName,
      [this]() { return fpromise::make_ok_promise(FillInspectData()); });
}

inspect::Inspector AccessibilityBridge::FillInspectData() {
  inspect::Inspector inspector;

  // Add a node for each AXTree of which the accessibility bridge is aware.
  // The output for each tree has the following form:
  //
  // <tree id>:
  //  contents: <string representation of tree contents>
  //  parent_tree: <tree id of this tree's parent, if it has one>
  //  parent_node: <node id of this tree's parent, if it has one>
  for (const auto& ax_tree : ax_trees_) {
    const ui::AXTree* ax_tree_ptr = ax_tree.second.get();

    inspect::Node inspect_node =
        inspector.GetRoot().CreateChild(ax_tree_ptr->GetAXTreeID().ToString());

    inspect_node.CreateString(kSemanticTreeContentsInspectPropertyName,
                              ax_tree_ptr->ToString(), &inspector);

    auto tree_id_and_connection =
        tree_connections_.find(ax_tree_ptr->GetAXTreeID());
    if (tree_id_and_connection != tree_connections_.end()) {
      const TreeConnection& connection = tree_id_and_connection->second;
      inspect_node.CreateString(kParentTreeInspectPropertyName,
                                connection.parent_tree_id.ToString(),
                                &inspector);
      inspect_node.CreateUint(kParentNodeInspectPropertyName,
                              connection.parent_node_id, &inspector);
    }

    inspector.emplace(std::move(inspect_node));
  }

  return inspector;
}

AccessibilityBridge::~AccessibilityBridge() {
  InterruptPendingActions();
  ax_trees_.clear();
}

void AccessibilityBridge::AddNodeToOffsetMapping(
    const ui::AXTree* tree,
    const ui::AXNodeData& node_data) {
  auto ax_tree_id = tree->GetAXTreeID();
  auto offset_container_id = GetOffsetContainerId(tree, node_data);
  offset_container_children_[std::make_pair(ax_tree_id, offset_container_id)]
      .insert(std::make_pair(ax_tree_id, node_data.id));
}

void AccessibilityBridge::RemoveNodeFromOffsetMapping(
    const ui::AXTree* tree,
    const ui::AXNodeData& node_data) {
  auto offset_container_children_it =
      offset_container_children_.find(std::make_pair(
          tree->GetAXTreeID(), GetOffsetContainerId(tree, node_data)));
  if (offset_container_children_it != offset_container_children_.end()) {
    offset_container_children_it->second.erase(
        std::make_pair(tree->GetAXTreeID(), node_data.id));
  }
}

void AccessibilityBridge::TryCommit() {
  if (commit_inflight_ || (to_delete_.empty() && to_update_.empty()) ||
      ShouldHoldCommit()) {
    return;
  }

  // Deletions come before updates because first the nodes are deleted, and
  // then we update the parents to no longer point at them.
  for (auto& batch : to_delete_) {
    semantic_tree_->DeleteSemanticNodes(std::move(batch));
  }
  to_delete_.clear();

  for (auto& batch : to_update_) {
    semantic_tree_->UpdateSemanticNodes(std::move(batch));
  }
  to_update_.clear();

  semantic_tree_->CommitUpdates(
      fit::bind_member(this, &AccessibilityBridge::OnCommitComplete));
  commit_inflight_ = true;
}

void AccessibilityBridge::OnCommitComplete() {
  // TODO(https://crbug.com/1134737): Separate updates of atomic updates and
  // don't allow all of them to be in the same commit.
  commit_inflight_ = false;
  TryCommit();
}

void AccessibilityBridge::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  // No need to process events if Fuchsia is not receiving them.
  if (!enable_semantic_updates_)
    return;

  const auto& id = details.ax_tree_id;
  if (!UpdateAXTreeID(id))
    return;

  auto ax_tree_it = ax_trees_.find(id);
  ui::AXSerializableTree* ax_tree;
  if (ax_tree_it == ax_trees_.end()) {
    auto new_tree = std::make_unique<ui::AXSerializableTree>();
    ax_tree = new_tree.get();
    ax_tree->AddObserver(this);
    ax_trees_[id] = std::move(new_tree);
  } else {
    ax_tree = ax_tree_it->second.get();
  }

  // Updates to AXTree must be applied first.
  for (const ui::AXTreeUpdate& update : details.updates) {
    if (!ax_tree->Unserialize(update)) {
      // If this fails, it is a fatal error that will cause an early exit.
      std::move(on_error_callback_).Run(ZX_ERR_INTERNAL);
      return;
    }
  }

  // Events to fire after tree has been updated.
  for (const ui::AXEvent& event : details.events) {
    if (event.event_type == ax::mojom::Event::kHitTestResult &&
        pending_hit_test_callbacks_.find(event.action_request_id) !=
            pending_hit_test_callbacks_.end()) {
      fuchsia::accessibility::semantics::Hit hit;
      hit.set_node_id(
          id_mapper_->ToFuchsiaNodeID(ax_tree->GetAXTreeID(), event.id, false));

      // Run the pending callback with the hit.
      pending_hit_test_callbacks_[event.action_request_id](std::move(hit));
      pending_hit_test_callbacks_.erase(event.action_request_id);
    } else if (event_received_callback_for_test_ &&
               event.event_type == ax::mojom::Event::kEndOfTest) {
      std::move(event_received_callback_for_test_).Run();
    }
  }
}

void AccessibilityBridge::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    OnAccessibilityActionRequestedCallback callback) {
  ui::AXActionData action_data = ui::AXActionData();

  // The requested action is not supported.
  if (!ConvertAction(action, &action_data.action)) {
    callback(false);
    return;
  }

  auto ax_id = id_mapper_->ToAXNodeID(node_id);
  if (!ax_id) {
    // Fuchsia is targeting a node that does not exist.
    callback(false);
    return;
  }

  action_data.target_node_id = ax_id->second;

  if (action == fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN) {
    ui::AXNode* node = nullptr;
    auto ax_tree_it = ax_trees_.find(ax_id->first);
    if (ax_tree_it != ax_trees_.end())
      node = ax_tree_it->second->GetFromId(action_data.target_node_id);

    if (!node) {
      callback(false);
      return;
    }

    // The scroll-to-make-visible action expects coordinates in the local
    // coordinate space of |node|. So, we need to translate node's bounds to the
    // origin.
    auto local_bounds = gfx::ToEnclosedRectIgnoringError(
        node->data().relative_bounds.bounds, kRectConversionError);
    local_bounds = gfx::Rect(local_bounds.size());

    action_data.target_rect = local_bounds;
    action_data.horizontal_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.vertical_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.scroll_behavior = ax::mojom::ScrollBehavior::kScrollIfVisible;
  }

  auto* frame =
      web_contents_->GetPrimaryMainFrame()->FromAXTreeID(ax_id->first);
  if (!frame) {
    // Fuchsia targeted a tree that does not exist.
    callback(false);
    return;
  }

  frame->AccessibilityPerformAction(action_data);
  callback(true);

  if (event_received_callback_for_test_) {
    // Perform an action with a corresponding event to signal the action has
    // been pumped through.
    action_data.action = ax::mojom::Action::kSignalEndOfTest;
    web_contents_->GetPrimaryMainFrame()->AccessibilityPerformAction(
        action_data);
  }
}

void AccessibilityBridge::HitTest(fuchsia::math::PointF local_point,
                                  HitTestCallback callback) {
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  gfx::Point point;
  float device_scale_factor = GetDeviceScaleFactor();
  point.set_x(local_point.x * device_scale_factor);
  point.set_y(local_point.y * device_scale_factor);
  action_data.target_point = point;
  action_data.hit_test_event_to_fire = ax::mojom::Event::kHitTestResult;
  pending_hit_test_callbacks_[action_data.request_id] = std::move(callback);

  web_contents_->GetPrimaryMainFrame()->AccessibilityPerformAction(action_data);
}

void AccessibilityBridge::OnSemanticsModeChanged(
    bool updates_enabled,
    OnSemanticsModeChangedCallback callback) {
  // TODO(https://crbug.com/1134591): Fix the case when enabling / disabling
  // semantics can lead to race conditions.
  if (enable_semantic_updates_ == updates_enabled)
    return callback();

  enable_semantic_updates_ = updates_enabled;
  if (updates_enabled) {
    id_mapper_ = std::make_unique<NodeIDMapper>();
    // The first call to AccessibilityEventReceived after this call will be
    // the entire semantic tree.
    web_contents_->EnableWebContentsOnlyAccessibilityMode();
  } else {
    // The SemanticsManager will clear all state in this case, which is
    // mirrored here.
    ui::AXMode mode = web_contents_->GetAccessibilityMode();
    mode.set_mode(ui::AXMode::kWebContents, false);
    web_contents_->SetAccessibilityMode(mode);
    to_delete_.clear();
    to_update_.clear();
    commit_inflight_ = false;
    ax_trees_.clear();
    tree_connections_.clear();
    frame_id_to_tree_id_.clear();
    InterruptPendingActions();
  }

  // Notify the SemanticsManager that this request was handled.
  callback();
}

void AccessibilityBridge::OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) {
  DCHECK(tree);
  DCHECK(node);

  AddNodeToOffsetMapping(tree, node->data());
}

void AccessibilityBridge::OnNodeWillBeDeleted(ui::AXTree* tree,
                                              ui::AXNode* node) {
  DCHECK(tree);
  DCHECK(node);

  // Remove the node from its offset container's list of children.
  RemoveNodeFromOffsetMapping(tree, node->data());

  // Also remove the mapping from deleted node to its offset children.
  offset_container_children_.erase(
      std::make_pair(tree->GetAXTreeID(), node->data().id));
}

void AccessibilityBridge::OnNodeDeleted(ui::AXTree* tree, int32_t node_id) {
  DCHECK(tree);

  AppendToDeleteList(
      id_mapper_->ToFuchsiaNodeID(tree->GetAXTreeID(), node_id, false));
}

void AccessibilityBridge::OnNodeDataChanged(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  DCHECK(tree);

  // If this node's offset container has changed, then we should remove it from
  // its old offset container's offset children and add it to its new offset
  // container's children.
  if (old_node_data.relative_bounds.offset_container_id !=
      new_node_data.relative_bounds.offset_container_id) {
    RemoveNodeFromOffsetMapping(tree, old_node_data);
    AddNodeToOffsetMapping(tree, new_node_data);
  }

  // If this node's bounds have changed, then we should update its offset
  // children's transforms to reflect the new bounds.
  if (old_node_data.relative_bounds.bounds ==
      new_node_data.relative_bounds.bounds) {
    return;
  }

  auto offset_container_children_it = offset_container_children_.find(
      std::make_pair(tree->GetAXTreeID(), old_node_data.id));

  if (offset_container_children_it == offset_container_children_.end())
    return;

  for (auto offset_child_id : offset_container_children_it->second) {
    auto* child_node = tree->GetFromId(offset_child_id.second);
    if (!child_node)
      continue;

    auto child_node_data = child_node->data();

    // If the offset container for |child_node| does NOT change during this
    // atomic update, then the update produced here will be correct.
    //
    // If the offset container for |child_node| DOES change during this atomic
    // update, then depending on the order of the individual node updates, the
    // update we produce here could be incorrect. However, in that case,
    // OnAtomicUpdateFinished() will see a change for |child_node|. By the time
    // that OnAtomicUpdateFinished() is called, offset_container_children_ will
    // be correct, so we can simply overwrite the existing update.
    auto* fuchsia_node =
        EnsureAndGetUpdatedNode(tree->GetAXTreeID(), child_node->data().id,
                                /*replace_existing=*/true);
    DCHECK(fuchsia_node);
  }
}

void AccessibilityBridge::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  DCHECK(tree);

  if (root_changed)
    MaybeDisconnectTreeFromParentTree(tree);

  const bool is_main_frame_tree =
      tree->GetAXTreeID() ==
      web_contents_->GetPrimaryMainFrame()->GetAXTreeID();
  if (is_main_frame_tree)
    root_id_ = tree->root()->id();

  // Changes included here are only nodes that are still on the tree. Since this
  // indicates the end of an atomic update, it is safe to assume that these
  // nodes will not change until the next change arrives. Nodes that would be
  // deleted are already gone, which means that all updates collected here in
  // |to_update_| are going to be executed after |to_delete_|.
  for (const ui::AXTreeObserver::Change& change : changes) {
    const auto& node = change.node->data();

    // Get the updated fuchsia representation of the node. It's possible that
    // there's an existing update for this node from OnNodeDataChanged(). This
    // update may not have the correct offset container and/or transform, so we
    // should replace it.
    auto* fuchsia_node = EnsureAndGetUpdatedNode(tree->GetAXTreeID(), node.id,
                                                 /*replace_existing=*/true);
    DCHECK(fuchsia_node);

    if (node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
      const auto child_tree_id = ui::AXTreeID::FromString(
          node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
      tree_connections_[child_tree_id] = {node.id, tree->GetAXTreeID(), false};
    }
  }

  UpdateTreeConnections();
  UpdateFocus();
  // TODO(https://crbug.com/1134737): Separate updates of atomic updates and
  // don't allow all of them to be in the same commit.
  TryCommit();
}

void AccessibilityBridge::InterruptPendingActions() {
  // Acknowledge to the SemanticsManager if any actions have not been handled
  // upon destruction time or when semantic updates have been disabled.
  for (auto& callback : pending_hit_test_callbacks_) {
    fuchsia::accessibility::semantics::Hit hit;
    callback.second(std::move(hit));
  }
  pending_hit_test_callbacks_.clear();
}

float AccessibilityBridge::GetDeviceScaleFactor() {
  if (device_scale_factor_override_for_test_) {
    return *device_scale_factor_override_for_test_;
  }

  return window_tree_host_->scenic_scale_factor();
}

ui::AXSerializableTree* AccessibilityBridge::ax_tree_for_test() {
  if (ax_trees_.empty())
    return nullptr;

  return ax_trees_.cbegin()->second.get();
}

void AccessibilityBridge::UpdateTreeConnections() {
  std::vector<ui::AXTreeID> connections_to_remove;
  for (auto& kv : tree_connections_) {
    ui::AXSerializableTree* child_tree = nullptr;
    auto it = ax_trees_.find(kv.first);
    if (it != ax_trees_.end())
      child_tree = it->second.get();

    ui::AXSerializableTree* parent_tree = nullptr;
    ui::AXNode* ax_node = nullptr;

    const auto& parent_ax_tree_id = kv.second.parent_tree_id;
    auto parent_it = ax_trees_.find(parent_ax_tree_id);
    if (parent_it != ax_trees_.end())
      parent_tree = parent_it->second.get();

    if (parent_tree) {
      ax_node = parent_tree->GetFromId(kv.second.parent_node_id);
      if (ax_node) {
        ax_node = ax_node->HasStringAttribute(
                      ax::mojom::StringAttribute::kChildTreeId)
                      ? ax_node
                      : nullptr;
      }
    }

    if (!child_tree && (!parent_tree || !ax_node)) {
      // Both the child tree and the parent tree are gone, so this connection is
      // no longer relevant.
      connections_to_remove.push_back(kv.first);
      continue;
    }

    if (!child_tree || (!parent_tree || !ax_node)) {
      // Only one side of the connection is still valid, so mark the trees as
      // disconnected and wait for either the connection to be done again or the
      // other side to drop.
      kv.second.is_connected = false;
      continue;
    }

    if (kv.second.is_connected)
      continue;  // No work to do, trees connected and present.

    auto* fuchsia_node =
        EnsureAndGetUpdatedNode(parent_ax_tree_id, ax_node->id(),
                                /*replace_existing=*/false);
    DCHECK(fuchsia_node);
    // Now, the connection really happens:
    // This node, from the parent tree, will have a child that points to the
    // root of the child tree.
    auto child_tree_root_id = id_mapper_->ToFuchsiaNodeID(
        child_tree->GetAXTreeID(), child_tree->root()->id(), false);
    fuchsia_node->mutable_child_ids()->push_back(child_tree_root_id);
    kv.second.is_connected = true;  // Trees are connected!
  }

  for (const auto& to_delete : connections_to_remove) {
    tree_connections_.erase(to_delete);
  }
}

void AccessibilityBridge::UpdateFocus() {
  auto new_focused_node = GetFocusedNodeId();
  if (!new_focused_node && !last_focused_node_id_)
    return;  // no node in focus, no new node in focus.

  const bool focus_changed = last_focused_node_id_ != new_focused_node;

  if (new_focused_node) {
    // If the new focus is the same as the old focus, we only want to set the
    // value in the node if it is part of the current update, meaning that its
    // data changed. This makes sure that it contains the focus information. If
    // it is not part of the current update, no need to send this information,
    // as it is redundant.
    auto* node = focus_changed
                     ? EnsureAndGetUpdatedNode(new_focused_node->first,
                                               new_focused_node->second,
                                               /*replace_existing=*/false)
                     : GetNodeIfChangingInUpdate(new_focused_node->first,
                                                 new_focused_node->second);
    if (node)
      node->mutable_states()->set_has_input_focus(true);
  }

  if (last_focused_node_id_) {
    auto* node = focus_changed
                     ? EnsureAndGetUpdatedNode(last_focused_node_id_->first,
                                               last_focused_node_id_->second,
                                               /*replace_existing=*/false)
                     : nullptr /*already updated above*/;
    if (node)
      node->mutable_states()->set_has_input_focus(false);
  }

  last_focused_node_id_ = std::move(new_focused_node);
}

bool AccessibilityBridge::ShouldHoldCommit() {
  const auto& main_frame_tree_id =
      web_contents_->GetPrimaryMainFrame()->GetAXTreeID();
  auto main_tree_it = ax_trees_.find(main_frame_tree_id);
  if (main_tree_it == ax_trees_.end()) {
    // The main tree is not present yet, commit should be held.
    return true;
  }

  // Make sure that all trees are reachable from the main frame semantic tree.
  // If a tree is not reachable, this means that when committed it would result
  // in a dangling tree, which is not valid.
  for (const auto& kv : ax_trees_) {
    const ui::AXTreeID& tree_id = kv.first;
    if (tree_id == main_frame_tree_id)
      continue;

    auto it = tree_connections_.find(tree_id);
    if (it == tree_connections_.end())
      return true;
  }

  for (const auto& kv : tree_connections_) {
    if (!kv.second.is_connected) {
      // Trees are not connected, which means that a node is pointing to
      // something that is not present yet. Since this causes an invalid tree,
      // the commit should be held.
      return true;
    }
  }
  return false;
}

void AccessibilityBridge::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  frame_id_to_tree_id_.erase(render_frame_host->GetGlobalId());

  const auto& id = render_frame_host->GetAXTreeID();
  auto it = ax_trees_.find(id);
  if (it != ax_trees_.end()) {
    it->second.get()->Destroy();
    MaybeDisconnectTreeFromParentTree(it->second.get());
    ax_trees_.erase(it);
    UpdateTreeConnections();
    TryCommit();
  }
}

bool AccessibilityBridge::UpdateAXTreeID(const ui::AXTreeID& tree_id) {
  auto* frame = content::RenderFrameHost::FromAXTreeID(tree_id);
  if (!frame)
    return false;

  auto frame_id = frame->GetGlobalId();
  DCHECK(frame_id);
  auto frame_iter = frame_id_to_tree_id_.find(frame_id);
  if (frame_iter == frame_id_to_tree_id_.end()) {
    // This is the first time this frame was seen. Save its AXTreeID.
    frame_id_to_tree_id_[frame_id] = tree_id;
    return true;
  }

  // This frame already exists. Check if the AXTreeID of this frame has changed.
  if (frame_iter->second == tree_id)
    return true;  // No updates needed.

  const auto& old_tree_id = frame_iter->second;
  id_mapper_->UpdateAXTreeIDForCachedNodeIDs(old_tree_id, tree_id);
  auto old_tree_iter = ax_trees_.find(old_tree_id);
  if (old_tree_iter != ax_trees_.end()) {
    // This AXTree has changed its AXTreeID. Update the map with the new key.
    auto data = std::move(old_tree_iter->second);
    ax_trees_.erase(old_tree_iter);
    ax_trees_[tree_id] = std::move(data);

    // If this tree is connected to a parent tree or is the parent tree of
    // another tree, also update its ID in the tree connections map.
    auto connected_tree_iter = tree_connections_.find(old_tree_id);
    if (connected_tree_iter != tree_connections_.end()) {
      auto data = std::move(connected_tree_iter->second);
      tree_connections_.erase(connected_tree_iter);
      tree_connections_[tree_id] = std::move(data);
      MaybeDisconnectTreeFromParentTree(ax_trees_[tree_id].get());
    }
    for (auto& kv : tree_connections_) {
      if (kv.second.parent_tree_id == old_tree_id)
        kv.second.parent_tree_id = tree_id;
    }
  }

  frame_iter->second = tree_id;
  return true;
}

void AccessibilityBridge::MaybeDisconnectTreeFromParentTree(ui::AXTree* tree) {
  const auto& key = tree->GetAXTreeID();
  auto it = tree_connections_.find(key);
  if (it != tree_connections_.end())
    it->second.is_connected = false;
}

absl::optional<AccessibilityBridge::AXNodeID>
AccessibilityBridge::GetFocusedNodeId() const {
  const auto& main_frame_tree_id =
      web_contents_->GetPrimaryMainFrame()->GetAXTreeID();
  const auto main_tree_it = ax_trees_.find(main_frame_tree_id);
  if (main_tree_it == ax_trees_.end())
    return absl::nullopt;

  const ui::AXSerializableTree* main_tree = main_tree_it->second.get();
  DCHECK(main_tree);
  const ui::AXTreeID& focused_tree_id = main_tree->data().focused_tree_id;
  if (focused_tree_id == ui::AXTreeIDUnknown())
    return absl::nullopt;

  const auto focused_tree_it = ax_trees_.find(focused_tree_id);
  if (focused_tree_it == ax_trees_.end())
    return absl::nullopt;

  const ui::AXSerializableTree* focused_tree = focused_tree_it->second.get();
  DCHECK(focused_tree);

  return GetFocusFromThisOrDescendantFrame(focused_tree);
}

absl::optional<AccessibilityBridge::AXNodeID>
AccessibilityBridge::GetFocusFromThisOrDescendantFrame(
    const ui::AXSerializableTree* tree) const {
  DCHECK(tree);
  const auto focused_node_id = tree->data().focus_id;
  const auto* node = tree->GetFromId(focused_node_id);
  const auto root_id = tree->root() ? tree->root()->id() : ui::kInvalidAXNodeID;
  if (!node) {
    if (root_id != ui::kInvalidAXNodeID)
      return std::make_pair(tree->GetAXTreeID(), root_id);

    return absl::nullopt;
  }

  if (node->data().HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) {
    const auto child_tree_id =
        ui::AXTreeID::FromString(node->data().GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId));
    const auto child_tree_it = ax_trees_.find(child_tree_id);
    if (child_tree_it != ax_trees_.end())
      return GetFocusFromThisOrDescendantFrame(child_tree_it->second.get());
  }

  return std::make_pair(tree->GetAXTreeID(), node->id());
}

void AccessibilityBridge::AppendToDeleteList(uint32_t node_id) {
  if (to_delete_.empty() || to_delete_.back().size() == kMaxNodesPerDelete) {
    to_delete_.emplace_back();
  }
  to_delete_.back().push_back(std::move(node_id));
}

void AccessibilityBridge::AppendToUpdateList(
    fuchsia::accessibility::semantics::Node node) {
  if (to_update_.empty() || to_update_.back().size() == kMaxNodesPerUpdate) {
    to_update_.emplace_back();
  }
  to_update_.back().push_back(std::move(node));
}

fuchsia::accessibility::semantics::Node*
AccessibilityBridge::GetNodeIfChangingInUpdate(const ui::AXTreeID& tree_id,
                                               ui::AXNodeID node_id) {
  auto fuchsia_node_id = id_mapper_->ToFuchsiaNodeID(tree_id, node_id, false);

  for (auto& update_batch : to_update_) {
    auto result =
        std::find_if(update_batch.rbegin(), update_batch.rend(),
                     [&fuchsia_node_id](
                         const fuchsia::accessibility::semantics::Node& node) {
                       return node.node_id() == fuchsia_node_id;
                     });
    if (result != update_batch.rend())
      return &(*result);
  }

  return nullptr;
}

fuchsia::accessibility::semantics::Node*
AccessibilityBridge::EnsureAndGetUpdatedNode(const ui::AXTreeID& tree_id,
                                             ui::AXNodeID node_id,
                                             bool replace_existing) {
  auto* fuchsia_node = GetNodeIfChangingInUpdate(tree_id, node_id);
  if (fuchsia_node && !replace_existing)
    return fuchsia_node;

  auto ax_tree_it = ax_trees_.find(tree_id);
  if (ax_tree_it == ax_trees_.end())
    return nullptr;

  auto* tree = ax_tree_it->second.get();
  auto* ax_node = tree->GetFromId(node_id);
  if (!ax_node)
    return nullptr;

  int32_t offset_container_id = GetOffsetContainerId(tree, ax_node->data());
  const auto* container = tree->GetFromId(offset_container_id);
  DCHECK(container);

  const bool is_main_frame_tree =
      tree->GetAXTreeID() ==
      web_contents_->GetPrimaryMainFrame()->GetAXTreeID();
  const bool is_root = is_main_frame_tree ? node_id == root_id_ : false;
  float device_scale_factor =
      ax_node->id() == tree->root()->id() ? GetDeviceScaleFactor() : 0.0f;
  auto new_fuchsia_node =
      AXNodeDataToSemanticNode(*ax_node, *container, tree_id, is_root,
                               device_scale_factor, id_mapper_.get());

  if (replace_existing && fuchsia_node) {
    *fuchsia_node = std::move(new_fuchsia_node);
    return fuchsia_node;
  }

  AppendToUpdateList(std::move(new_fuchsia_node));
  return &to_update_.back().back();
}
