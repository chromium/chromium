// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include <algorithm>

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

// TODO(https://crbug.com/973095): Update this value based on average and
// maximum sizes of serialized Semantic Nodes.
constexpr size_t kMaxNodesPerUpdate = 16;

// Error allowed for each edge when converting from gfx::RectF to gfx::Rect.
constexpr float kRectConversionError = 0.5;

}  // namespace

AccessibilityBridge::AccessibilityBridge(
    fuchsia::accessibility::semantics::SemanticsManager* semantics_manager,
    fuchsia::ui::views::ViewRef view_ref,
    content::WebContents* web_contents,
    base::OnceCallback<void(zx_status_t)> on_error_callback)
    : binding_(this),
      web_contents_(web_contents),
      on_error_callback_(std::move(on_error_callback)) {
  DCHECK(web_contents_);
  Observe(web_contents_);

  semantics_manager->RegisterViewForSemantics(
      std::move(view_ref), binding_.NewBinding(), semantic_tree_.NewRequest());
  semantic_tree_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "SemanticTree disconnected";
    std::move(on_error_callback_).Run(ZX_ERR_INTERNAL);
  });
}

AccessibilityBridge::~AccessibilityBridge() {
  InterruptPendingActions();
  ax_trees_.clear();
}

void AccessibilityBridge::TryCommit() {
  if (commit_inflight_ || (to_delete_.empty() && to_update_.empty()) ||
      ShouldHoldCommit())
    return;

  // Deletions come before updates because first the nodes are deleted, and
  // then we update the parents to no longer point at them.
  if (!to_delete_.empty())
    semantic_tree_->DeleteSemanticNodes(std::move(to_delete_));

  size_t start = 0;
  while (start < to_update_.size()) {
    // TODO(https://crbug.com/1134727): AccessibilityBridge must respect FIDL
    // size limits.
    size_t end =
        start + std::min(kMaxNodesPerUpdate, to_update_.size() - start);
    decltype(to_update_) batch;
    std::move(to_update_.begin() + start, to_update_.begin() + end,
              std::back_inserter(batch));
    semantic_tree_->UpdateSemanticNodes(std::move(batch));
    start = end;
  }
  semantic_tree_->CommitUpdates(
      fit::bind_member(this, &AccessibilityBridge::OnCommitComplete));
  commit_inflight_ = true;
  to_delete_.clear();
  to_update_.clear();
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

    action_data.target_rect = gfx::ToEnclosedRectIgnoringError(
        node->data().relative_bounds.bounds, kRectConversionError);
    action_data.horizontal_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.vertical_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.scroll_behavior = ax::mojom::ScrollBehavior::kScrollIfVisible;
  }

  web_contents_->GetMainFrame()
      ->FromAXTreeID(ax_id->first)
      ->AccessibilityPerformAction(action_data);
  callback(true);

  if (event_received_callback_for_test_) {
    // Perform an action with a corresponding event to signal the action has
    // been pumped through.
    action_data.action = ax::mojom::Action::kSignalEndOfTest;
    web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
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

  web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
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

void AccessibilityBridge::OnNodeDeleted(ui::AXTree* tree, int32_t node_id) {
  to_delete_.push_back(
      id_mapper_->ToFuchsiaNodeID(tree->GetAXTreeID(), node_id, false));
}

void AccessibilityBridge::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  if (root_changed)
    MaybeDisconnectTreeFromParentTree(tree);

  const bool is_main_frame_tree =
      tree->GetAXTreeID() == web_contents_->GetMainFrame()->GetAXTreeID();
  if (is_main_frame_tree)
    root_id_ = tree->root()->id();

  // Changes included here are only nodes that are still on the tree. Since this
  // indicates the end of an atomic update, it is safe to assume that these
  // nodes will not change until the next change arrives. Nodes that would be
  // deleted are already gone, which means that all updates collected here in
  // |to_update_| are going to be executed after |to_delete_|.
  for (const ui::AXTreeObserver::Change& change : changes) {
    const auto& node = change.node->data();
    const bool is_root = is_main_frame_tree ? node.id == root_id_ : false;
    to_update_.push_back(AXNodeDataToSemanticNode(node, tree->GetAXTreeID(),
                                                  is_root, id_mapper_.get()));
    if (node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
      const auto child_tree_id = ui::AXTreeID::FromString(
          node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
      tree_connections_[child_tree_id] = {node.id, tree->GetAXTreeID(), false};
    }
  }
  UpdateTreeConnections();
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
  return web_contents_->GetRenderWidgetHostView()->GetDeviceScaleFactor();
}

const ui::AXSerializableTree* AccessibilityBridge::ax_tree_for_test() {
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

    auto fuchsia_node = AXNodeDataToSemanticNode(
        ax_node->data(), parent_ax_tree_id, false, id_mapper_.get());

    // Now, the connection really happens:
    // This node, from the parent tree, will have a child that points to the
    // root of the child tree.
    auto child_tree_root_id = id_mapper_->ToFuchsiaNodeID(
        child_tree->GetAXTreeID(), child_tree->root()->id(), false);
    fuchsia_node.mutable_child_ids()->push_back(child_tree_root_id);
    to_update_.push_back(std::move(fuchsia_node));
    kv.second.is_connected = true;  // Trees are connected!
  }

  for (const auto& to_delete : connections_to_remove) {
    tree_connections_.erase(to_delete);
  }
}

bool AccessibilityBridge::ShouldHoldCommit() {
  const auto& main_frame_tree_id = web_contents_->GetMainFrame()->GetAXTreeID();
  auto main_tree_it = ax_trees_.find(main_frame_tree_id);
  if (main_tree_it == ax_trees_.end()) {
    // The main tree is not present yet, commit should be held.
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

  frame_id_to_tree_id_.erase(render_frame_host->GetGlobalFrameRoutingId());

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

  auto frame_id = frame->GetGlobalFrameRoutingId();
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
